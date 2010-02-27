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


#include "bmwb.h"


static char msg[ ME_ERROR_MSG_MAX_COUNT ];


static int setup_ai( void );
static int setup_aos( void );
static int setup_dios( void );


static unsigned char dio_states[ 4 ] = { 0, 0, 0, 0 };


typedef struct {
    int    unit;
    double min;
    double max;
    int    cnt;
} range_T;    

range_T ai_range[ ] = { { ME_UNIT_VOLT, -10.0, 10.0, 65535 },
                        { ME_UNIT_VOLT,   0.0, 10.0, 65535 },
                        { ME_UNIT_VOLT,  -2.5,  2.5, 65535 },
                        { ME_UNIT_VOLT,   0.0,  2.5, 65535 } };

range_T ao_range[ ] = { { ME_UNIT_VOLT, -10.0, 10.0, 65535 },
                        { ME_UNIT_VOLT, -10.0, 10.0, 65535 },
                        { ME_UNIT_VOLT, -10.0, 10.0, 65535 },
                        { ME_UNIT_VOLT, -10.0, 10.0, 65535 } };


static struct {
    int       id;
    range_T * range;
} sub_dev[ ] = { { ME_TYPE_DI,  NULL         },
                 { ME_TYPE_DO,  NULL         },
                 { ME_TYPE_DIO, NULL         },
                 { ME_TYPE_DIO, NULL         },
                 { ME_TYPE_AI,  ai_range     },
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
    char buf[ 32 ];


    /* Try to open library */

	if ( meOpen( ME_OPEN_NO_FLAGS ) != ME_ERRNO_SUCCESS )
	{
		meErrorGetLastMessage( msg, sizeof msg );
		sprintf( bmwb.error_msg, "Failed to initialize Meilhaus driver: %s.\n",
                 msg );
        return 1;
	}
  
#if ! defined BMWB_TEST

    lower_permissions( );

    /* Try to get a lock on the whole device */

    if ( meLockDevice( DEV_ID, ME_LOCK_SET, ME_LOCK_DEVICE_NO_FLAGS )
                                                          != ME_ERRNO_SUCCESS )
    {
		meErrorGetLastMessage( msg, sizeof msg );
		sprintf( bmwb.error_msg, "Failed to obtain lock on Meilhaus card: "
                 "%s.\n", msg );
        meClose( ME_CLOSE_NO_FLAGS );
		lower_permissions( );
        return 1;
    }

    /* Check that it's a ME-4680 card */

    if ( meQueryNameDevice( DEV_ID, buf, sizeof buf ) != ME_ERRNO_SUCCESS )
    {
		meErrorGetLastMessage( msg, sizeof msg );
        meLockDevice( DEV_ID, ME_LOCK_RELEASE, ME_LOCK_DEVICE_NO_FLAGS );
        meClose( ME_CLOSE_NO_FLAGS );
		lower_permissions( );
        sprintf( bmwb.error_msg, "Can't determine device type: %s.\n", msg );
        return 1;
    }

    if ( strncmp( buf, "ME-4680", 7 ) )
    {
        meLockDevice( DEV_ID, ME_LOCK_RELEASE, ME_LOCK_DEVICE_NO_FLAGS );
        meClose( ME_CLOSE_NO_FLAGS );
		lower_permissions( );
        sprintf( bmwb.error_msg, "Device not a ME-4680 card.\n" );
        return 1;
    }

    /* Initialize the sub-devices */

    if ( setup_ai( ) )
    {
        meLockDevice( DEV_ID, ME_LOCK_RELEASE, ME_LOCK_DEVICE_NO_FLAGS );
        meClose( ME_CLOSE_NO_FLAGS );
		lower_permissions( );
        return 1;
    }

    if ( setup_aos( ) )
    {
        meLockDevice( DEV_ID, ME_LOCK_RELEASE, ME_LOCK_DEVICE_NO_FLAGS );
        meClose( ME_CLOSE_NO_FLAGS );
		lower_permissions( );
        return 1;
    }

    if ( setup_dios( ) )
    {
        meLockDevice( DEV_ID, ME_LOCK_RELEASE, ME_LOCK_DEVICE_NO_FLAGS );
        meClose( ME_CLOSE_NO_FLAGS );
		lower_permissions( );
        return 1;
    }

	lower_permissions( );
#endif

	return 0;
}


/*------------------------------------*
 * Releases the Meilhaus ME-4680 card
 *------------------------------------*/

int
meilhaus_finish( void )
{

#if ! defined BMWB_TEST
	raise_permissions( );
    meLockDevice( DEV_ID, ME_LOCK_RELEASE, ME_LOCK_DEVICE_NO_FLAGS );
    lower_permissions( );
#endif

	if ( meClose( ME_CLOSE_NO_FLAGS ) != ME_ERRNO_SUCCESS )
	{
		meErrorGetLastMessage( msg, sizeof msg );
		sprintf( bmwb.error_msg, "Failed to close Meilhaus driver: %s.\n",
                 msg );
        return 1;
	}

    return 0;
}


/*----------------------------------------------------------------------*
 * Checks that the AI of the card can be found and reads out the ranges
 * (initialization can not be done here since the mode it's used with
 * changes)
 *----------------------------------------------------------------------*/

static int
setup_ai( void )
{
    int i;
    int n;


	raise_permissions( );

    if ( meQuerySubdeviceByType( DEV_ID, AI, sub_dev[ AI ].id,
                                 ME_SUBTYPE_ANY, &n ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to query subdevice %d: %s.\n",
                 AI, msg );
        lower_permissions( );
        return 1;
    }

    if ( AI != n )
    {
        sprintf( bmwb.error_msg, "AI subdevice %d does not have expected "
                 "type.\n", AI );
        lower_permissions( );
        return 1;
    }

    /* Determine the number of output ranges of the AI */

    if ( meQueryNumberRanges( DEV_ID, AI, ME_UNIT_ANY, &n )
                                                         != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to query number of output ranges "
                 "for AI subdevice %d: %s.\n", AI, msg );
        lower_permissions( );
        return 1;
    }

    if ( n != 4 )
    {
        sprintf( bmwb.error_msg, "Unexpected number of output ranges "
                 "for AI subdevice %d.\n", AI );
        lower_permissions( );
        return 1;
    }

    for ( i = 0; i < n; i++ )
        if ( meQueryRangeInfo( DEV_ID, AI, 0, &sub_dev[ AI ].range[ i ].unit,
                               &sub_dev[ AI ].range[ i ].min,
                               &sub_dev[ AI ].range[ i ].max,
                               &sub_dev[ AI ].range[ i ].cnt )
                                                          != ME_ERRNO_SUCCESS )
        {
            meErrorGetLastMessage( msg, sizeof msg );
            sprintf( bmwb.error_msg, "Failed to query range %d for subdevice "
                     "%d: %s.\n", i, AI, msg );
            lower_permissions( );
            return 1;
        }

    lower_permissions( );

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


    for ( i = AO_0; i <= AO_3; i++ )
    {
        if ( meQuerySubdeviceByType( DEV_ID, i, sub_dev[ i ].id,
                                     ME_SUBTYPE_ANY, &n ) != ME_ERRNO_SUCCESS )
        {
            meErrorGetLastMessage( msg, sizeof msg );
            sprintf( bmwb.error_msg, "Failed to query subdevice %d: %s.\n",
                     i, msg );
            return 1;
        }

        if ( i != n )
        {
            sprintf( bmwb.error_msg, "AO subdevice %d does not have expected "
                     "type.\n", i );
            return 1;
        }

        /* Determine the output range of the AO - this cards AOs have only a
           single range (+/-10V), so no need to check the number of ranges */

        if ( meQueryRangeInfo( DEV_ID, i, 0, &sub_dev[ i ].range->unit,
                               &sub_dev[ i ].range->min,
                               &sub_dev[ i ].range->max,
                               &sub_dev[ i ].range->cnt ) != ME_ERRNO_SUCCESS )
        {
            meErrorGetLastMessage( msg, sizeof msg );
            sprintf( bmwb.error_msg, "Failed to query range for AO subdevice "
                     "%d: %s.\n", i, msg );
            return 1;
        }


        /* Set up the AO - output is supposed to happen directly on a call
           of meIOSingle() */

        if ( meIOSingleConfig( DEV_ID, i, 0, 0,
                               ME_REF_AO_GROUND, ME_TRIG_CHAN_DEFAULT,
                               ME_TRIG_TYPE_SW, ME_VALUE_NOT_USED,
                               ME_IO_SINGLE_CONFIG_NO_FLAGS )
                                                           != ME_ERRNO_SUCCESS )
        {
            meErrorGetLastMessage( msg, sizeof msg );
            sprintf( bmwb.error_msg, "Failed to set up AO subdevice %d: %s.\n",
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


    for ( i = DIO_A; i <= DIO_D; i++ )
    {
        if ( meQuerySubdeviceByType( DEV_ID, i, sub_dev[ i ].id,
                                     ME_SUBTYPE_ANY, &n ) != ME_ERRNO_SUCCESS )
        {
            meErrorGetLastMessage( msg, sizeof msg );
            sprintf( bmwb.error_msg, "Failed to query subdevice %d: %s.\n",
                     i, msg );
            return 1;
        }

        if ( i != n )
        {
            sprintf( bmwb.error_msg, "DIO subdevice %d does not have expected "
                     "type.\n", i );
            return 1;
        }

        if ( meIOSingleConfig( DEV_ID, i, 0,
                               i == 0 ? ME_SINGLE_CONFIG_DIO_INPUT :
                                        ME_SINGLE_CONFIG_DIO_OUTPUT,
                               ME_REF_NONE, ME_TRIG_CHAN_DEFAULT,
                               ME_TRIG_TYPE_SW, ME_VALUE_NOT_USED,
                               ME_IO_SINGLE_CONFIG_DIO_BYTE )
                                                         != ME_ERRNO_SUCCESS )
        {
            meErrorGetLastMessage( msg, sizeof msg );
            sprintf( bmwb.error_msg, "Failed to set up DIO subdevice %d: %s.\n",
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


	raise_permissions( );

    /* Find proper range for expected signal */

    if ( meQueryRangeByMinMax( DEV_ID, AI, sub_dev[ AI ].range[ 0 ].unit,
                               &min, &max, &sub_dev[ AI ].range[ 0 ].cnt,
                               &range ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to find range for AI subdevice "
                 "%d: %s.\n", AI, msg );
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
        sprintf( bmwb.error_msg, "Failed to set up AI subdevice %d: %s.\n",
                 AI, msg );
        lower_permissions( );
        return 1;
    }

    /* Do the measurement */

    if ( meIOSingle( &list, 1, ME_IO_SINGLE_NO_FLAGS ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to measure on AI subdevice %d: %s.\n",
                 AI, msg );
        lower_permissions( );
        return 1;
    }

    /* Convert measured digital value to physical value */

    if ( meUtilityDigitalToPhysical( min, max, sub_dev[ AI ].range[ range ].cnt,
                                     list.iValue, ME_MODULE_TYPE_MULTISIG_NONE,
                                     ME_VALUE_NOT_USED, val )
                                                          != ME_ERRNO_SUCCESS )

    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failed to convert measured value from AI "
                 "subdevice %d: %s.\n", AI, msg );
        lower_permissions( );
        return 1;
    }

    lower_permissions( );

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
        sprintf( bmwb.error_msg, "Invalid AO argument for meilhaus_ao().\n" );
        return 1;
    }

    /* Make sure the requested voltage is within the range covered by the AO
       (if it's not more than 0.1% above the limits then reduce to the limit) */

    if (    (    val < sub_dev[ ao ].range->min
              && fabs( ( val - sub_dev[ ao ].range->min )
                       / sub_dev[ ao ].range->min ) > 0.001 )
         || (    val > sub_dev[ ao ].range->max
              && fabs( ( val - sub_dev[ ao ].range->max )
                       / sub_dev[ ao ].range->max ) > 0.001 ) )
    {
        sprintf( bmwb.error_msg, "Invalid voltage argument for "
                 "meilhaus_ao().\n" );
        return 1;
    }

    if ( val < sub_dev[ ao ].range->min )
        val = sub_dev[ ao ].range->min;
    else if ( val > sub_dev[ ao ].range->max )
        val = sub_dev[ ao ].range->max;

    /* Convert volts to an integer value as expected by the library */

    if ( meUtilityPhysicalToDigital( sub_dev[ ao ].range->min,
                                     sub_dev[ ao ].range->max,
                                     sub_dev[ ao ].range->cnt, val,
                                     &list.iValue ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Failure to convert physical to digital "
                 "value: %s.\n", msg );
        return 1;
    }

#if ! defined BMWB_TEST
    /* Make the AO output the voltage */

	raise_permissions( );

    if ( meIOSingle( &list, 1, ME_IO_SINGLE_NO_FLAGS ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "D/A failure for AO_%d: %s.\n",
                 ao, msg );
        lower_permissions( );
        return 1;
    }

    lower_permissions( );
#endif

    return 0;
}

/*--------------------------------------------*
 * Returns the bit pattern measured at the DI
 *--------------------------------------------*/

int
meilhaus_dio_in( int             dio,
                 unsigned char * val )
{
    meIOSingle_t list = { DEV_ID, dio, 0, ME_DIR_INPUT, 0, 0,
                          ME_IO_SINGLE_TYPE_DIO_BYTE, 0 };


    /* Make sure the requested DIO exists and is in output mode (only the
       first one is) and that the second argument is reasonable */

    if ( dio != DIO_A || val == NULL )
    {
        sprintf( bmwb.error_msg, "Invalid argument for meilhaus_dio_in().\n" );
        return 1;
    }

#if ! defined BMWB_TEST
	raise_permissions( );

    if ( meIOSingle( &list, 1, ME_IO_SINGLE_NO_FLAGS ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Error while reading from DIO %d: %s.\n",
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


/*-----------------------------------------------*
 * Returns the bit pattern currently set at a DO
 *-----------------------------------------------*/

int
meilhaus_dio_out_state( int             dio,
                        unsigned char * val )
{
    /* Make sure the requested DIO is in input mode and the second argument
       is reasonable */

    if ( dio < DIO_B || dio > DIO_D || val == NULL )
    {
        sprintf( bmwb.error_msg, "Invalid argument for "
                 "meilhaus_dio_out_state().\n" );
        return 1;
    }

    *val = dio_states[ dio ];
    return 0;
}


/*-------------------------------*
 * Outputs a bit pattern at a DO
 *-------------------------------*/

int
meilhaus_dio_out( int           dio,
                  unsigned char val )
{
    meIOSingle_t list = { DEV_ID, dio, 0, ME_DIR_OUTPUT, val, 0,
                          ME_IO_SINGLE_TYPE_DIO_BYTE, 0 };


    /* Make sure the requested DIO is in input mode */

    if ( dio < DIO_B || dio > DIO_D )
    {
        sprintf( bmwb.error_msg, "Invalid argument for meilhaus_dio_out().\n" );
        return 1;
    }

#if ! defined BMWB_TEST
	raise_permissions( );

    /* Fetch a value from the card */

    if ( meIOSingle( &list, 1, ME_IO_SINGLE_NO_FLAGS ) != ME_ERRNO_SUCCESS )
    {
        meErrorGetLastMessage( msg, sizeof msg );
        sprintf( bmwb.error_msg, "Error while writing to DIO %d: %s.\n",
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
