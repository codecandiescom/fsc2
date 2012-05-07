/*
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
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


#include "bmwb.h"


#define AI_MIN_VOLTAGE  -10.0
#define AI_MAX_VOLTAGE   10.0


static char msg[ ME_ERROR_MSG_MAX_COUNT ];


static int setup_ai( void );
static int setup_aos( void );
static int setup_dios( void );


/* Last bit patterns output via the DOs and DIOs */

static unsigned char dio_states[ 4 ] = { 0, 0, 0, 0 };


typedef struct {
    int    unit;
    double min;
    double max;
    int    cnt;
} range_T;    

range_T ai_range = { ME_UNIT_VOLT, 0.0, 0.0, 0 };

range_T ao_range[ ] = { { ME_UNIT_VOLT, -10.0, 10.0, 65536 },
                        { ME_UNIT_VOLT, -10.0, 10.0, 65536 },
                        { ME_UNIT_VOLT, -10.0, 10.0, 65536 },
                        { ME_UNIT_VOLT, -10.0, 10.0, 65536 } };


static struct {
    int       id;
    range_T * range;
} sub_dev[ ] = { { ME_TYPE_DI,  NULL         },
                 { ME_TYPE_DO,  NULL         },
                 { ME_TYPE_DIO, NULL         },
                 { ME_TYPE_DIO, NULL         },
                 { ME_TYPE_AI,  &ai_range    },
                 { ME_TYPE_AO,  ao_range     },
                 { ME_TYPE_AO,  ao_range + 1 },
                 { ME_TYPE_AO,  ao_range + 2 },
                 { ME_TYPE_AO,  ao_range + 3 } };


/*---------------------------------------*
 * Initializes the Meilhaus ME-4680 card
 *---------------------------------------*/

int
meilhaus_init( void )
{
#if defined BMWB_TEST

    return 0;

#else

    char buf[ 32 ];
    int dev_count;
    int is_locked = 0;


    raise_permissions( );

    /* Try to initialize the library, thereby opening the device file */

	if ( meOpen( ME_OPEN_NO_FLAGS ) != ME_ERRNO_SUCCESS )
	{
		meErrorGetLastMessage( msg, sizeof msg );
		sprintf( bmwb.error_msg, "Failed to initialize Meilhaus driver: %s.",
                 msg );
		lower_permissions( );
        return 1;
	}
  
    /* Find out how many devices are present */

    if ( meQueryNumberDevices( &dev_count ) != ME_ERRNO_SUCCESS )
	{
		meErrorGetLastMessage( msg, sizeof msg );
		sprintf( bmwb.error_msg, "Failed to determine number of Meilhaus "
                 "devices: %s", msg );
        goto failed_itinitalization;
	}

    /* If there's none there's nothing further we can do... */

    if ( dev_count == 0 )
    {
		sprintf( bmwb.error_msg, "No Meilhaus cards found." );
        goto failed_itinitalization;
	}

    /* Try to get a lock on the whole device */

    if ( meLockDevice( DEV_ID, ME_LOCK_SET, ME_LOCK_DEVICE_NO_FLAGS )
                                                          != ME_ERRNO_SUCCESS )
    {
		meErrorGetLastMessage( msg, sizeof msg );
		sprintf( bmwb.error_msg, "Failed to obtain lock on Meilhaus card: "
                 "%s", msg );
        goto failed_itinitalization;
    }

    is_locked = 1;

    /* Check that it's the required ME-4670 or ME-4680 card */

    if ( meQueryNameDevice( DEV_ID, buf, sizeof buf ) != ME_ERRNO_SUCCESS )
    {
		meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Can't determine device type: %s", msg );
        goto failed_itinitalization;
    }

    if (    strcmp( buf, "ME-4670S" )
         && strcmp( buf, "ME-4670IS" )
         && strcmp( buf, "ME-4680S" )
         && strcmp( buf, "ME-4680IS" ) )
    {
        sprintf( bmwb.error_msg, "Device not a ME-4670s, ME-4670is, "
                 "ME-4680s or ME-4680is card." );
        goto failed_itinitalization;
    }

    /* Initialize all sub-devices */

    if ( setup_ai( ) || setup_aos( ) || setup_dios( ) )
        goto failed_itinitalization;

    /* Everything seems to be fine so far */

	lower_permissions( );
	return 0;

  failed_itinitalization:

    if ( is_locked )
        meLockDevice( DEV_ID, ME_LOCK_RELEASE, ME_LOCK_DEVICE_NO_FLAGS );
    meClose( ME_CLOSE_NO_FLAGS );

    lower_permissions( );
    return 1;

#endif
}


/*----------------------------*
 * Releases the Meilhaus card
 *----------------------------*/

int
meilhaus_finish( void )
{
#if ! defined BMWB_TEST

	raise_permissions( );

    meLockDevice( DEV_ID, ME_LOCK_RELEASE, ME_LOCK_DEVICE_NO_FLAGS );

	if ( meClose( ME_CLOSE_NO_FLAGS ) != ME_ERRNO_SUCCESS )
	{
		meErrorGetLastMessage( msg, sizeof msg );
        lower_permissions( );
		sprintf( bmwb.error_msg, "Failed to close Meilhaus driver: %s", msg );
        return 1;
	}

    lower_permissions( );

#endif

    return 0;
}


/*----------------------------------------------------------------------*
 * Checks that the AI of the card can be found and reads out the ranges
 * (initialization can not be done here since the mode it's used with
 * will change between single and continuous operation)
 *----------------------------------------------------------------------*/

static int
setup_ai( void )
{
    int n;


    if ( meQuerySubdeviceByType( DEV_ID, AI, sub_dev[ AI ].id,
                                 ME_SUBTYPE_ANY, &n ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to query subdevice %d: %s", AI, msg );
        return 1;
    }

    if ( AI != n )
    {
        sprintf( bmwb.error_msg, "AI subdevice %d does not have expected "
                 "type.", AI );
        return 1;
    }

    /* Determine the number of input ranges of the AI */

    if ( meQueryNumberRanges( DEV_ID, AI, ME_UNIT_ANY, &n )
                                                         != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to query number of input ranges "
                 "for AI subdevice %d: %s", AI, msg );
        return 1;
    }

    if ( n != 4 )
    {
        sprintf( bmwb.error_msg, "Unexpected number of input ranges "
                 "for AI subdevice %d.", AI );
        return 1;
    }

    return 0;
}


/*----------------------------------*
 * Initializes the AOs of the card
 *----------------------------------*/

static int
setup_aos( void )
{
    int i;
    int n;


    /* Loop over all 4 AOs */

    for ( i = AO_0; i <= AO_3; i++ )
    {
        if ( meQuerySubdeviceByType( DEV_ID,
                                     i,
                                     sub_dev[ i ].id,
                                     ME_SUBTYPE_ANY,
                                     &n ) != ME_ERRNO_SUCCESS )
        {
            meErrorGetLastMessage( msg, sizeof msg );
            sprintf( bmwb.error_msg, "Failed to query subdevice %d: %s",
                     i, msg );
            return 1;
        }

        if ( i != n )
        {
            sprintf( bmwb.error_msg, "AO subdevice %d does not have expected "
                     "type.", i );
            return 1;
        }

        /* Determine the output range of the AO - the cards AOs have only a
           single range (+/-10V), so no need to check the number of ranges */

        if ( meQueryRangeInfo( DEV_ID,
                               i,
                               0,
                               &sub_dev[ i ].range->unit,
                               &sub_dev[ i ].range->min,
                               &sub_dev[ i ].range->max,
                               &sub_dev[ i ].range->cnt ) != ME_ERRNO_SUCCESS )
        {
            meErrorGetLastMessage( msg, sizeof msg );
            sprintf( bmwb.error_msg, "Failed to query range for AO subdevice "
                     "%d: %s", i, msg );
            return 1;
        }

        /* Set up the AO - output is supposed to happen directly on a call
           of meIOSingle() */

        if ( meIOSingleConfig( DEV_ID,
                               i,
                               0,
                               0,
                               ME_REF_AO_GROUND, ME_TRIG_CHAN_DEFAULT,
                               ME_TRIG_TYPE_SW, ME_VALUE_NOT_USED,
                               ME_IO_SINGLE_CONFIG_NO_FLAGS )
                                                           != ME_ERRNO_SUCCESS )
        {
            meErrorGetLastMessage( msg, sizeof msg );
            sprintf( bmwb.error_msg, "Failed to set up AO subdevice %d: %s",
                     i, msg );
            return 1;
        }
    }

    return 0;
}


/*----------------------------------*
 * Initializes the DIOs of the card
 *----------------------------------*/

static int
setup_dios( void )
{
    int i;
    int n;


    /* Loop over all 4 DIOs */

    for ( i = DIO_A; i <= DIO_D; i++ )
    {
        if ( meQuerySubdeviceByType( DEV_ID, i, sub_dev[ i ].id,
                                     ME_SUBTYPE_ANY, &n ) != ME_ERRNO_SUCCESS )
        {
            meErrorGetLastMessage( msg, sizeof msg );
            sprintf( bmwb.error_msg, "Failed to query subdevice %d: %s",
                     i, msg );
            return 1;
        }

        if ( i != n )
        {
            sprintf( bmwb.error_msg, "DIO subdevice %d does not have expected "
                     "type.", i );
            return 1;
        }

        /* Set up the DIOs (the first DIO for input, all others for output)
           - output or input is supposed to happen directly on a call of
           meIOSingle() */

        if ( meIOSingleConfig( DEV_ID, i, 0,
                               i == DIO_A ? ME_SINGLE_CONFIG_DIO_INPUT :
                                            ME_SINGLE_CONFIG_DIO_OUTPUT,
                               ME_REF_NONE,
                               ME_TRIG_CHAN_NONE,
                               ME_TRIG_TYPE_NONE,
                               ME_TRIG_EDGE_NONE,
                               ME_IO_SINGLE_NO_FLAGS ) != ME_ERRNO_SUCCESS )
        {
            meErrorGetLastMessage( msg, sizeof msg );
            sprintf( bmwb.error_msg, "Failed to set up DIO subdevice %d: %s",
                     i, msg );
            return 1;
        }
    }

    return 0;
}


/*-----------------------------------------------------*
 * Does a single measurement on one of the AI channels
 *-----------------------------------------------------*/

int
meilhaus_ai_single( int      channel,
                    double   min,
                    double   max,
                    double * val )
{
    int range;
    meIOSingle_t list = { DEV_ID, AI, channel, ME_DIR_INPUT, 0, 0,
                          ME_IO_SINGLE_TYPE_NO_FLAGS, 0 };


    if ( channel < 0 || channel > 8 )
    {
        sprintf( bmwb.error_msg, "Invalid 'channel' argument to "
                 "meilhaus_ai_single()." );
        return 1;
    }

    if ( min < AI_MIN_VOLTAGE || min >= max || max > AI_MAX_VOLTAGE )
    {
        sprintf( bmwb.error_msg, "Invalid 'min' or 'max' argument to "
                 "meilhaus_ai_single()." );
        return 1;
    }

    if ( ! val )
    {
        sprintf( bmwb.error_msg, "Invalid 'val' argument to "
                 "meilhaus_ai_single()." );
        return 1;
    }

#if ! defined BMWB_TEST

	raise_permissions( );

    /* Make sure the AI is in a well-defined state */

    if ( meIOResetSubdevice( DEV_ID, AI, ME_VALUE_NOT_USED )
                                                          != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to do reset for AI subdevice "
                 "%d: %s", AI, msg );
        lower_permissions( );
        return 1;
    }

    /* Find proper range for expected signal */

    if ( meQueryRangeByMinMax( DEV_ID, AI, sub_dev[ AI ].range->unit,
                               &min, &max, &sub_dev[ AI ].range->cnt,
                               &range ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to find range for AI subdevice "
                 "%d: %s", AI, msg );
        lower_permissions( );
        return 1;
    }
        
    /* Configure the AI for single measurement */

    if ( meIOSingleConfig( DEV_ID, AI, channel, range,
                           ME_REF_AI_GROUND, ME_TRIG_CHAN_DEFAULT,
                           ME_TRIG_TYPE_SW, ME_VALUE_NOT_USED,
                           ME_IO_SINGLE_CONFIG_NO_FLAGS ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to set up AI subdevice %d, channel "
                 "%d: %s", AI, channel, msg );
        lower_permissions( );
        return 1;
    }

    /* Do the measurement */

    if ( meIOSingle( &list, 1, ME_IO_SINGLE_NO_FLAGS ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to measure on AI subdevice %d, "
                 "channel %d: %s", AI, channel, msg );
        lower_permissions( );
        return 1;
    }

    /* Convert measured digital value to physical value */

    if ( meUtilityDigitalToPhysical( min, max, sub_dev[ AI ].range->cnt,
                                     list.iValue, ME_MODULE_TYPE_MULTISIG_NONE,
                                     ME_VALUE_NOT_USED, val )
                                                          != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to convert measured value from AI "
                 "subdevice %d, channel %d: %s", AI, channel, msg );
        lower_permissions( );
        return 1;
    }

    lower_permissions( );

#endif

    return 0;
}


/*-----------------------------------------------------*
 * Does a parallel measurement of two curves, each of
 * length 'len' at a rate given by 'freq'.
 *-----------------------------------------------------*/

/* Size of buffer for transfer of data between card and program - the
   card seems to have a FIFO of 2048 bytes, so make the buffer as
   large as that */

#define AI_BUF_SIZ  ( 2048 / sizeof( int ) )

int
meilhaus_ai_get_curves( int      x_channel,
                        double * x_data,
                        double   x_min,
                        double   x_max,
                        int      y_channel,
                        double * y_data,
                        double   y_min,
                        double   y_max,
                        size_t   len,
                        double   freq )
{
    int x_range,
        y_range;
    meIOStreamConfig_t channel_list[ 2 ];
    meIOStreamTrigger_t trigger;
    meIOStreamStart_t start_list;
    meIOStreamStop_t stop_list;
    size_t still_to_read = 2 * len;
    double f = 0.0;


    /* Check all arguments */

    if (    x_channel < 0 || x_channel >= 8
         || y_channel < 0 || y_channel >= 8
         || x_channel == y_channel )
    {
        sprintf( bmwb.error_msg, "Invalid x or y channel argument to "
                 "meilhaus_ai_get_curves()." );
        return 1;
    }

    if ( ! x_data )
    {
        sprintf( bmwb.error_msg, "Invalid 'x_data' argument to "
                 "meilhaus_ai_get_curves()." );
        return 1;
    }

    if ( x_min < AI_MIN_VOLTAGE || x_min >= x_max || x_max > AI_MAX_VOLTAGE )
    {
        sprintf( bmwb.error_msg, "Invalid 'x_min' or 'x_max' argument to "
                 "meilhaus_ai_single()." );
        return 1;
    }

    if ( ! y_data )
    {
        sprintf( bmwb.error_msg, "Invalid 'y_data' argument to "
                 "meilhaus_ai_get_curves()." );
        return 1;
    }

    if ( y_min < AI_MIN_VOLTAGE || y_min >= y_max || y_max > AI_MAX_VOLTAGE )
    {
        sprintf( bmwb.error_msg, "Invalid 'y_min' or 'y_max' argument to "
                 "meilhaus_ai_single()." );
        return 1;
    }

    if ( ! len )
    {
        sprintf( bmwb.error_msg, "Invalid 'len' argument to "
                 "meilhaus_ai_single()." );
        return 1;
    }

    if ( freq <= 0.0 )
    {
        sprintf( bmwb.error_msg, "Invalid 'freq' argument to "
                 "meilhaus_ai_single()." );
        return 1;
    }

#if ! defined BMWB_TEST

	raise_permissions( );

    /* Make sure the AI is in a well-defined state */

    if ( meIOResetSubdevice( DEV_ID, AI, ME_VALUE_NOT_USED )
                                                          != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to do reset for AI subdevice "
                 "%d: %s", AI, msg );
        lower_permissions( );
        return 1;
    }

    /* Find proper ranges for the expected x- and y-signals */

    if ( meQueryRangeByMinMax( DEV_ID, AI, sub_dev[ AI ].range->unit,
                               &x_min, &x_max, &sub_dev[ AI ].range->cnt,
                               &x_range ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to find x range for AI subdevice "
                 "%d: %s", AI, msg );
        lower_permissions( );
        return 1;
    }
        
    if ( meQueryRangeByMinMax( DEV_ID, AI, sub_dev[ AI ].range->unit,
                               &y_min, &y_max, &sub_dev[ AI ].range->cnt,
                               &y_range ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to find y range for AI subdevice "
                 "%d: %s", AI, msg );
        lower_permissions( );
        return 1;
    }

    /* Assemble the channel list */

    channel_list[ 0 ].iChannel      = x_channel;
    channel_list[ 0 ].iStreamConfig = x_range;
    channel_list[ 0 ].iRef          = ME_REF_AI_DIFFERENTIAL;
    channel_list[ 0 ].iFlags        = ME_IO_STREAM_CONFIG_TYPE_NO_FLAGS;

    channel_list[ 1 ].iChannel      = y_channel;
    channel_list[ 1 ].iStreamConfig = y_range;
    channel_list[ 1 ].iRef          = ME_REF_AI_DIFFERENTIAL;
    channel_list[ 1 ].iFlags        = ME_IO_STREAM_CONFIG_TYPE_NO_FLAGS;

    /* Set up the trigger structure */

    memset( &trigger, 0, sizeof trigger );

    /* Acquisition is to be started immediately after the meIOStreamStart()
       call without any trigger */

    trigger.iAcqStartTrigType   = ME_TRIG_TYPE_SW;
    trigger.iAcqStartTrigChan   = ME_TRIG_CHAN_DEFAULT;

    if ( meIOStreamFrequencyToTicks( DEV_ID, AI, ME_TIMER_ACQ_START, &f,
                                     &trigger.iAcqStartTicksLow,
                                     &trigger.iAcqStartTicksHigh,
                                     ME_IO_FREQUENCY_TO_TICKS_NO_FLAGS )
                                                          != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to convert ACQ_START frequency for "
                 "AI subdevice %d: %s", AI, msg );
        lower_permissions( );
        return 1;
    }

    /* Scans are to be started via a timer with the requested frequency */

    trigger.iScanStartTrigType  = ME_TRIG_TYPE_TIMER;

    f = freq;
    if ( meIOStreamFrequencyToTicks( DEV_ID, AI, ME_TIMER_SCAN_START, &f,
                                     &trigger.iScanStartTicksLow,
                                     &trigger.iScanStartTicksHigh,
                                     ME_IO_FREQUENCY_TO_TICKS_NO_FLAGS )
                                                          != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to convert SCAN_START frequency for "
                 "AI subdevice %d: %s", AI, msg );
        lower_permissions( );
        return 1;
    }

    /* 2 conversions are to be done in each scan, also controlled by the
       timer */

    trigger.iConvStartTrigType  = ME_TRIG_TYPE_TIMER;
    f = 2 * freq;

    if ( meIOStreamFrequencyToTicks( DEV_ID, AI, ME_TIMER_SCAN_START, &f,
                                     &trigger.iConvStartTicksLow,
                                     &trigger.iConvStartTicksHigh,
                                     ME_IO_FREQUENCY_TO_TICKS_NO_FLAGS )
                                                          != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to convert CONV_START frequency for "
                 "AI subdevice %d: %s", AI, msg );
        lower_permissions( );
        return 1;
    }

    /* Scans don't need a stop trigger, the whole acquisition is to be
       stopped when 'len' values for each of the two channels have been
       converted */

    trigger.iScanStopTrigType   = ME_TRIG_TYPE_NONE;
    trigger.iAcqStopTrigType    = ME_TRIG_TYPE_COUNT;
    trigger.iAcqStopCount       = 2 * len;

    /* Configure the AI accordingly */

    if ( meIOStreamConfig( DEV_ID, AI, channel_list, 2, &trigger, 
                           AI_BUF_SIZ, ME_IO_STREAM_CONFIG_NO_FLAGS )
                                                          != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to configure AI subdevice "
                 "%d for streaming: %s", AI, msg );
        lower_permissions( );
        return 1;
    }

    /* Assemble the start and stop list and start the acquisition */

    start_list.iDevice    = DEV_ID;
    start_list.iSubdevice = AI;
    start_list.iStartMode = ME_START_MODE_NONBLOCKING;
    start_list.iTimeOut   = 0;
    start_list.iFlags     = ME_IO_STREAM_START_TYPE_NO_FLAGS;
    start_list.iErrno     = ME_VALUE_NOT_USED;

    stop_list.iDevice    = DEV_ID;
    stop_list.iSubdevice = AI;
    stop_list.iStopMode  = ME_STOP_MODE_IMMEDIATE;
    stop_list.iFlags     = ME_IO_STREAM_STOP_TYPE_NO_FLAGS;
    stop_list.iErrno     = ME_VALUE_NOT_USED;

    if ( meIOStreamStart( &start_list, 1, ME_IO_STREAM_START_NO_FLAGS )
                                                          != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to start acquisition for AI subdevice "
                 "%d, channels %d and %d: %s", AI, x_channel, y_channel, msg );
        lower_permissions( );
        return 1;
    }

    /* Read all the data we expect to receive */

    do
    {
        int buf[ AI_BUF_SIZ ];
        int count;
        size_t i;


        count = still_to_read < AI_BUF_SIZ ? still_to_read : AI_BUF_SIZ;

        if ( meIOStreamRead( DEV_ID, AI, ME_READ_MODE_BLOCKING, buf, &count,
                             ME_IO_STREAM_READ_NO_FLAGS ) != ME_ERRNO_SUCCESS )
        {
            meErrorGetLastMessage( msg, sizeof msg );
            meIOStreamStop( &stop_list, 1, ME_IO_STREAM_STOP_NO_FLAGS );
            sprintf( bmwb.error_msg, "Acquisition failed for AI subdevice %d, "
                     "channels %d and %d: %s", AI, x_channel, y_channel, msg );
            lower_permissions( );
            return 1;
        }

        /* Convert the data - the data for the two channels alternate in
           the returned buffer, first x, then y. */

        for ( i = 0; i < ( size_t ) count / 2; i++ )
            if (    meUtilityDigitalToPhysical( x_min, x_max,
                                                sub_dev[ AI ].range->cnt,
                                                buf[ 2 * i ], 
                                                ME_MODULE_TYPE_MULTISIG_NONE,
                                                ME_VALUE_NOT_USED, x_data + i )
                                                          != ME_ERRNO_SUCCESS
                 || meUtilityDigitalToPhysical( y_min, y_max,
                                                sub_dev[ AI ].range->cnt,
                                                buf[ 2 * i + 1 ], 
                                                ME_MODULE_TYPE_MULTISIG_NONE,
                                                ME_VALUE_NOT_USED, x_data + i )
                                                           != ME_ERRNO_SUCCESS )
            {
                meErrorGetLastMessage( msg, sizeof msg );
                meIOStreamStop( &stop_list, 1, ME_IO_STREAM_STOP_NO_FLAGS );
                sprintf( bmwb.error_msg, "Failed to convert measured data "
                         "from AI subdevice %d: %s", AI, msg );
                lower_permissions( );
                return 1;
            }

        still_to_read -= count;

    } while ( still_to_read );

    if ( meIOStreamStop( &stop_list, 1, ME_IO_STREAM_STOP_NO_FLAGS )
                                                           != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to stop acquisition for AI subdevice "
                 "%d: %s", AI, msg );
        lower_permissions( );
        return 1;
    }

#endif

    return 0;
}


/*-------------------------------------*
 * Outputs a voltage at one of the AOs
 *-------------------------------------*/

int
meilhaus_ao( int    ao,
             double val )
{
    meIOSingle_t list = { DEV_ID, ao, 0, ME_DIR_OUTPUT, 0, 0,
                          ME_IO_SINGLE_TYPE_NO_FLAGS, 0 };


    /* Make sure the requested AO exists */

    if ( ao < AO_0 || ao > AO_3 )
    {
        sprintf( bmwb.error_msg, "Invalid 'ao' argument to meilhaus_ao()." );
        return 1;
    }

    /* Make sure the requested voltage is within the range covered by the AO
       (if it's not more than 0.1% above the limits then reduce to the limit) */

    if (    ! val
         || (    val < sub_dev[ ao ].range->min
              && fabs( ( val - sub_dev[ ao ].range->min )
                       / sub_dev[ ao ].range->min ) > 0.001 )
         || (    val > sub_dev[ ao ].range->max
              && fabs( ( val - sub_dev[ ao ].range->max )
                       / sub_dev[ ao ].range->max ) > 0.001 ) )
    {
        sprintf( bmwb.error_msg, "Invalid 'val' argument to meilhaus_ao()." );
        return 1;
    }

    if ( val < sub_dev[ ao ].range->min )
        val = sub_dev[ ao ].range->min;
    else if ( val > sub_dev[ ao ].range->max )
        val = sub_dev[ ao ].range->max;

#if ! defined BMWB_TEST

    /* Convert volts to an integer value as expected by the library */

    if ( meUtilityPhysicalToDigital( sub_dev[ ao ].range->min,
                                     sub_dev[ ao ].range->max,
                                     sub_dev[ ao ].range->cnt, val,
                                     &list.iValue ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failure to convert physical to digital "
                 "value: %s", msg );
        return 1;
    }

    /* Make the AO output the voltage */

	raise_permissions( );

    if ( meIOSingle( &list, 1, ME_IO_SINGLE_NO_FLAGS ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "D/A failure for AO_%d: %s", ao, msg );
        lower_permissions( );
        return 1;
    }

    lower_permissions( );
#endif

    return 0;
}


/*-------------------------------------------------------*
 * Returns the bit pattern measured at the digital input
 *-------------------------------------------------------*/

int
meilhaus_dio_in( int             dio,
                 unsigned char * val )
{
    meIOSingle_t list = { DEV_ID,                     /* device index        */
                          dio,                        /* subdevice index     */
                          0,                          /* subdevice channel   */
                          ME_DIR_INPUT,               /* read operation      */
                          0,                          /* contains result     */
                          ME_VALUE_NOT_USED,          /* no time-out         */
                          ME_IO_SINGLE_TYPE_NO_FLAGS, /* use "natural" width */
                          0                           /* for errno           */
                        };


    /* Make sure the requested DIO is in input mode (only the DIO_A is) and
       that the second argument is reasonable */

    if ( dio != DIO_A )
    {
        sprintf( bmwb.error_msg, "Invalid 'dio' argument to "
                 "meilhaus_dio_in()." );
        return 1;
    }

    if ( ! val )
    {
        sprintf( bmwb.error_msg, "Invalid 'val' argument to "
                 "meilhaus_dio_in()." );
        return 1;
    }

#if ! defined BMWB_TEST

	raise_permissions( );

    if ( meIOSingle( &list, 1, ME_IO_SINGLE_NO_FLAGS ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Error while reading from DIO %d: %s",
                 dio, msg );
        lower_permissions( );
        return 1;
    }

    lower_permissions( );

    *val = list.iValue;
#else
    *val = 0;
#endif

    return 0;
}


/*-----------------------------------------------------------*
 * Returns the bit pattern currently set at a digital output
 * (not measured but from has been set before)
 *-----------------------------------------------------------*/

int
meilhaus_dio_out_state( int             dio,
                        unsigned char * val )
{
    /* Make sure the requested DIO is in output mode (all but DIO_A are) and
       that the second argument is reasonable */

    if ( dio < DIO_B || dio > DIO_D )
    {
        sprintf( bmwb.error_msg, "Invalid 'dio' argument to "
                 "meilhaus_dio_out_state()." );
        return 1;
    }

    if ( ! val )
    {
        sprintf( bmwb.error_msg, "Invalid 'val' argument to "
                 "meilhaus_dio_out_state()." );
        return 1;
    }

    *val = dio_states[ dio ];
    return 0;
}


/*-----------------------------------------------------*
 * Outputs a bit pattern at one of the digital outputs
 *-----------------------------------------------------*/

int
meilhaus_dio_out( int           dio,
                  unsigned char val )
{
    meIOSingle_t list = { DEV_ID,                     /* device index        */
                          dio,                        /* subdevice index     */
                          0,                          /* subdevice channel   */
                          ME_DIR_OUTPUT,              /* write operation     */
                          val,                        /* value to output     */
                          ME_VALUE_NOT_USED,          /* no time-out         */
                          ME_IO_SINGLE_TYPE_NO_FLAGS, /* use "natural" width */
                          0                           /* for errno           */
                        };


    /* Make sure the requested DIO is in output mode (all but DIO_A are) */

    if ( dio < DIO_B || dio > DIO_D )
    {
        sprintf( bmwb.error_msg, "Invalid 'dio' argument to "
                 "meilhaus_dio_out()." );
        return 1;
    }

#if ! defined BMWB_TEST

	raise_permissions( );

    /* Send the value to the card */

    if ( meIOSingle( &list, 1, ME_IO_SINGLE_NO_FLAGS ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Error while writing to DIO %d: %s",
                 dio, msg );
        lower_permissions( );

        return 1;;
    }

    lower_permissions( );
#endif

    /* Store what we just output for later requests */

    dio_states[ dio ] = val;
    return 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
