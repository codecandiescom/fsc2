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


#include "fsc2_module.h"
#include "me_dio.conf"
#include <medriver.h>


#define DIO_MAX_CHANNEL  8

int config_flags[ ] = { ME_IO_SINGLE_CONFIG_DIO_BIT,
                        ME_IO_SINGLE_CONFIG_DIO_BYTE };
int query_flags[ ]  = { ME_CAPS_DIO_DIR_BIT,
                        ME_CAPS_DIO_DIR_BYTE };
const char *mode_name[ ] = { "8 1-bit DIO channels",
                             "1 8-bit DIO channel", };

enum DIO_MODE {
    DIO_BIT   = 8,
    DIO_BYTE  = 1,
};

enum DIO_DIR {
    DIO_IN      = 1,
    DIO_OUT     = 2
};


enum DIO_TYPE {
    DI          = 0,
    DO          = 1,
    DIO         = 2
};


int me_dio_init_hook(       void );
int me_dio_test_hook(       void );
int me_dio_exp_hook(        void );
int me_dio_end_of_exp_hook( void );


Var_T * dio_name(        Var_T * v );
Var_T * dio_reserve_dio( Var_T * v );
Var_T * dio_mode(        Var_T * v );
Var_T * dio_value(       Var_T * v );


static void me_dio_init( void );
static void me_dio_exit( void );
static unsigned long me_dio_in( int ch );
static void me_dio_out( int           ch,
                        unsigned long val );
static long me_dio_translate_channel( long channel );


struct ME_DIO {
    bool is_locked;
    char *reserved_by;
    int dev_no;
    int subdev_no;
    int type;
    bool dir_cap[ 2 ];
    int mode;
    int config_flags;
    int single_flags;
    int max_channels;
    unsigned int dir[ MAX_CHANNELS ];
};


static struct ME_DIO me_dio, me_dio_saved;


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
me_dio_init_hook( void )
{
    size_t i;


	Need_MEDRIVER = SET;

    me_dio.dev_no = DEVICE_NUMBER;
    me_dio.subdev_no = SUBDEVICE_NUMBER;
    switch ( DEVICE_TYPE )
    {
        case DI :
            me_dio.type = ME_TYPE_DI;
            break;

        case DO :
            me_dio.type = ME_TYPE_DO;
            break;

        case DIO :
            me_dio.type = ME_TYPE_DIO;
            break;

        default:
            print( FATAL, "Invalid device type in configiration file.\n" );
            return 0;
    }

    me_dio.max_channels = MAX_CHANNELS;
    me_dio.is_locked = UNSET;

    for ( i = 0; i < sizeof query_flags / sizeof *query_flags; i++ )
        me_dio.caps[ i ] = CAPABILITIES & query_flags[ i ] ? SET : UNSET;

    me_dio.reserved_by = NULL;

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
me_dio_test_hook( void )
{
    me_dio_saved = me_dio;
	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
me_dio_exp_hook( void )
{
    if ( me_dio_saved.reserved_by != NULL )
        me_dio_saved.reserved_by = CHAR_P T_free( me_dio_saved.reserved_by );

    me_dio = me_dio_saved;
    me_dio_init( );
	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
me_dio_end_of_exp_hook( void )
{
    me_dio_exit( );
    if ( me_dio.reserved_by != NULL )
        me_dio.reserved_by = CHAR_P T_free( me_dio.reserved_by );

	return 1;
}


/*----------------------------------------------------------------*
 * Function returns a string variable with the name of the device
 *----------------------------------------------------------------*/

Var_T *
dio_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*-----------------------------------------------------------*
 * The module allows to get locks on the DIO. This is done
 * so that another module can reserve a DIO for its own use
 * and the user can't accidentally mess around with the DIO.
 * To get a lock this function must be called with a pass-
 * phrase (usually the device name for the module requesting
 * the lock). If there's also a second argument it must be
 * boolean and if set the function will look the DIO,
 * otherwise it is unlocked.
 * The function never throws an exception, it just returns 1
 * on success (the requested DIO is now locked or unlocked)
 * or 0 if the lock state could not be changed. Trying to
 * lock twice with the same pass-phrase succeeds but is a
 * NOP.
 *-----------------------------------------------------------*/

Var_T *
dio_reserve_dio( Var_T * v )
{
    bool lock_state = SET;


    /* If there's no argument return a value indicating if the DIO is locked */

    if ( v == NULL )
        return vars_push( INT_VAR, me_dio.reserved_by ? 1L : 0L );

    /* Get the pass-phrase */

    if ( v->type != STR_VAR )
    {
        print( FATAL, "Argument isn't a string.\n" );
        THROW( EXCEPTION );
    }

    /* The next, optional argument tells if the DIO is to be locked or
       unlocked (if it's missing it defaults to locking the DIO) */

    if ( v->next != NULL )
    {
        lock_state = get_boolean( v->next );
        too_many_arguments( v->next );
    }

    /* Lock or unlock (after checking the pass-phrase) the DIO */

    if ( me_dio.reserved_by )
    {
        if ( lock_state == SET )
        {
            if ( ! strcmp( me_dio.reserved_by, v->val.sptr ) )
                return vars_push( INT_VAR, 1L );
            else
                return vars_push( INT_VAR, 0L );
        }
        else
        {
            if ( ! strcmp( me_dio.reserved_by, v->val.sptr ) )
            {
                me_dio.reserved_by = CHAR_P T_free( me_dio.reserved_by );
                return vars_push( INT_VAR, 1L );
            }
            else
                return vars_push( INT_VAR, 0L );
        }
    }

    if ( ! lock_state )
        return vars_push( INT_VAR, 1L );

    me_dio.reserved_by = T_strdup( v->val.sptr );
    return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------------------*
 * Function for querying or setting the mode the DIO. Modes are
 * either 1 or 8 (for the number of bits to be used).
 * If the DIO has been reserved, the first argument must be the
 * correct pass-phrase agreed upon in the lock operation.
 *--------------------------------------------------------------*/

Var_T *
dio_mode( Var_T * v )
{
    long mode;
    char *pass = NULL;
    int i;


    if ( FSC2_MODE == PREPARATIONS )
    {
        print( FATAL, "Function can only be used in EXPERIMENT section.\n" );
        THROW( EXCEPTION );
    }

    /* If the first argument is a string we assume it's a pass-phrase */

    if ( v != NULL && v->type == STR_VAR )
    {
        pass = v->val.sptr;
        if ( v->next == NULL )
        {
            print( FATAL, "Missing argument(s).\n" );
            THROW( EXCEPTION );
        }
        v = v->next;
    }

	/* If there's no (further) argument return the current mode */

	if ( v == NULL )
		return vars_push( INT_VAR, ( long ) num_bits[ me_dio.mode ] );

    /* Get and check the mode */

	mode = get_long( v, "DIO mode" );

    if ( mode != DIO_BIT && mode != DIO_BYTE )
    {
		print( FATAL, "Invalid mode, must be 1 or 8.\n" );
		THROW( EXCEPTION );
	}

    /* If the DIO is locked check the pass-phrase */

    if ( me_dio.reserved_by )
    {
        if ( pass == NULL )
        {
            print( FATAL, "DIO is reserved, phase-phrase required.\n" );
            THROW( EXCEPTION );
        }
        else if ( strcmp( me_dio.reserved_by, pass ) )
        {
            print( FATAL, "DIO is reserved, wrong phase-phrase.\n" );
            THROW( EXCEPTION );
        }
    }

    too_many_arguments( v );

    /* Set the new mode */

    if ( mode > ne_dio.max_channels )
    {
        print( FATAL, "Device does not support %s.\n",
               mode_name[ mode == DIO_BIT ? 0 : 1 ] );
        THROW( EXCEPTION );
    }

    me_dio.mode = mode;

    switch ( mode )
    {
        case DIO_BIT :
            if ( me_dio.caps[ 0 ] == 0 )
                me_dio.config_flags = ME_IO_SINGLE_CONFIG_DIO_BIT;
            else
                me_dio.config_flags = ME_IO_SINGLE_CONFIG_DIO_BYTE;
            me_dio.single_flags = ME_IO_SINGLE_TYPE_DIO_BIT;
            break;

        case DIO_BYTE :
            me_dio.config_flags = ME_IO_SINGLE_CONFIG_DIO_BYTE;
            me_dio.single_flags = ME_IO_SINGLE_TYPE_DIO_BYTE;
            break;
    }

    for ( i = 0; i < me_dio.max_channels; i++ )
        me_dio.dir[ i ] = DIO_UNKNOWN;

    return vars_push( INT_VAR, num_bits[ mode ] );
}


/*-------------------------------------------------------------
 *-------------------------------------------------------------*/

Var_T *
dio_value( Var_T * v )
{
    long ch;
    long val;
    unsigned long uval = 0;
    char *pass = NULL;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* If the first argument is a string we assume it's a pass-phrase */

    if ( v->type == STR_VAR )
    {
        pass = v->val.sptr;
        if ( v->next == NULL )
        {
            print( FATAL, "Missing arguments.\n" );
            THROW( EXCEPTION );
        }
        v = v->next;
    }

    /* Get and check the channel number */

    ch = me_dio_translate_channel( get_strict_long( v, "channel number" ) );

    /* If the DIO is locked check the pass-phrase */

    if ( me_dio.reserved_by )
    {
        if ( pass == NULL )
        {
            print( FATAL, "DIO is reserved, phase-phrase required.\n" );
            THROW( EXCEPTION );
        }
        else if ( strcmp( me_dio.reserved_by, pass ) )
        {
            print( FATAL, "DIO is reserved, wrong phase-phrase.\n" );
            THROW( EXCEPTION );
        }
    }

    if ( ch < 0 || ch >= me_dio.mode )
    {
        print( FATAL, "Invalid channel %ld\n", ch );
        THROW( EXCEPTION );
    }

    /* If there's no value to output read the input and return it to the user */

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        if ( me_dio.type == DO )
        {
            print( FATAL, "Device only allows output.\n" );
            THROW( EXCEPTION );
        }

        if ( FSC2_MODE == EXPERIMENT )
            uval = me_dio_in( ch );

        return vars_push( INT_VAR, ( long ) uval );
    }

    /* Otherwise get the value and check if it's reasonable */

    val = get_strict_long( v, "value to output" );

    too_many_arguments( v );

    if ( me_dio.type == DI )
    {
        print( FATAL, "Device only allows input.\n" );
        THROW( EXCEPTION );
    }

    uval = ( unsigned long ) val;

    if (    ( me_dio.mode == DIO_BIT   && uval > 1 )
         || ( me_dio.mode == DIO_BYTE  && uval > 0xFF ) )
    {
        if ( FSC2_MODE != EXPERIMENT )
        {
            print( FATAL, "Value of 0x%lx to be output is too large for "
                   "%d mode.\n", uval, num_bits[ me_dio.mode ] );
            THROW( EXCEPTION );
        }
        else
        {
            if ( me_dio.mode == DIO_BIT )
                uval &= 1;
            else ( me_dio.mode == DIO_BYTE )
                uval &= 0xFF;

            print( FATAL, "Value of 0x%lx to be output is too large for "
                   "%d mode, truncating it to %lu.\n", ( unsigned long ) val,
                   num_bits[ me_dio.mode ], uval );
        }
    }

    /* Output the value */

    if ( FSC2_MODE == EXPERIMENT )
        me_dio_out( ch, uval );

    return vars_push( INT_VAR, ( long ) uval );
}


/*-------------------------------------------------------------
 *-------------------------------------------------------------*/

static void
me_dio_init( void )
{
    int retval;
    int type;
    int subtype;
    size_t i;
    int caps;


    /* Try to find the device */

    if ( ( retval =
           	   meQuerySubdeviceType( DEVICE_NUMBER, SUBDEVICE_NUMBER,
                                     &type, &subtype ) ) != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetMessage( retval, msg, sizeof msg );
        print( FATAL, "Failed to locate DIO: %s\n", msg );
        THROW( EXCEPTION );
    }

    if ( type != me_dio.type || subtype != ME_SUBTYPE_SINGLE )
    {
        print( FATAL, "Invalid device type in configuration file.\n" );
        THROW( EXCEPTION );
    }

    /* Query capabilities */

    if ( ( retval =
              meQuerySubdeviceCaps( me_dio.dev_no, me_dio.subdev_no,
                                    &caps ) ) != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetMessage( retval, msg, sizeof msg );
        print( FATAL, "Failed to query DIO capabilities: %s\n", msg );
        THROW( EXCEPTION );
    }

    if ( caps != CAPABILITIES )
    {
        print( FATAL, "Invalid setting fir capabilities in configuration "
               "file.\n" );
        THROW( EXCEPTION );
    }

    /* Query the number of channels */

    if ( ( retval =
              meQueryNumberChannels( me_dio.dev_no, me_dio.subdev_no,
                                     &ch ) ) != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetMessage( retval, msg, sizeof msg );
        print( FATAL, "Failed to query number of DIO channels: %s\n", msg );
        THROW( EXCEPTION );
    }

    if ( ch != me_dio.max_channels )
    {
        print( FATAL, "Wrong maximum number of channels in configuration "
               "file.\n" );
        THROW( EXCEPTION );
    }

    /* Try to get a lock on the subdevice */

    if ( ( retval =
           	   meLockSubdevice( me_dio.dev_no, me_dio.subdev_no,
                                ME_LOCK_SET, ME_LOCK_SUBDEVICE_NO_FLAGS ) )
                                                          != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetMessage( retval, msg, sizeof msg );
        print( FATAL, "Failed to lock DIO: %s\n", msg );
        THROW( EXCEPTION );
    }

    me_dio.is_locked = SET;
}


/*-------------------------------------------------------------
 *-------------------------------------------------------------*/

static void
me_dio_exit( void )
{
    int retval;


    if ( me_dio.is_locked
         && ( retval =
           	    meLockSubdevice( me_dio.dev_no, me_dio.subdev_no,
                                 ME_LOCK_RELEASE, ME_LOCK_SUBDEVICE_NO_FLAGS ) )
                                                          != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetMessage( retval, msg, sizeof msg );
        print( FATAL, "Failed to unlock DIO: %s\n", msg );
    }

    me_dio.is_locked = UNSET;
}


/*-------------------------------------------------------------
 *-------------------------------------------------------------*/

static unsigned long
me_dio_in( int ch )
{
    int retval;
    meIOSingle_t list = { me_dio.dev_no, me_dio.subdev_no, ch,
                          ME_DIR_INPUT, 0, 0, me_dio.single_flags, 0 };


    /* Configure the channel if necessary */

    if (    me_dio.dir[ ch ] != DIO_IN
         && ( retval = meIOSingleConfig( me_dio.dev_no,
                                         me_dio.subdev_no,
                                         ch,
                                         ME_SINGLE_CONFIG_DIO_INPUT,
                                         0,
                                         0,
                                         0,
                                         0,
                                         me_dio.config_flags
                  ) )
                                                           != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetMessage( retval, msg, sizeof msg );
        print( FATAL, "Error while setting up DIO: %s\n", msg );
        THROW( EXCEPTION );
    }

    me_dio.dir[ ch ] = DIO_IN;

    /* Prepare input and send to DIO */

    if ( ( retval = meIOSingle( &list, 1, ME_IO_SINGLE_NO_FLAGS ) )
                                                          != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetMessage( list.iErrno, msg, sizeof msg );
        print( FATAL, "Error while reading from DIO: %s\n", msg );
        THROW( EXCEPTION );
    }

    return list.iValue;
}


/*-------------------------------------------------------------
 *-------------------------------------------------------------*/

static void
me_dio_out( int           ch,
            unsigned long val )
{
    int retval;
    meIOSingle_t list = { me_dio.dev_no, me_dio.subdev_no, ch,
                          ME_DIR_OUTPUT, val, 0, me_dio.single_flags, 0 };

    /* Configure the channel if necessary */

    if (    me_dio.dir[ ch ] != DIO_OUT
         && ( retval = meIOSingleConfig( me_dio.dev_no,
                                         me_dio.subdev_no,
                                         ch,
                                         ME_SINGLE_CONFIG_DIO_OUTPUT,
                                         0,
                                         0,
                                         0,
                                         0,
                                         me_dio.config_flags
                  ) )
                                                          != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetMessage( retval, msg, sizeof msg );
        print( FATAL, "Error while setting up DIO: %s\n", msg );
        THROW( EXCEPTION );
    }

    me_dio.dir[ ch ] = DIO_OUT;

    /* Prepare output and send to DIO */

    if ( ( retval = meIOSingle( &list, 1, ME_IO_SINGLE_NO_FLAGS ) )
                                                           != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetMessage( list.iErrno, msg, sizeof msg );
        print( FATAL, "Error while writing to DIO: %s\n", msg );
        THROW( EXCEPTION );
    }
}


/*--------------------------------------------------------------*
 * The function is used to translate back and forth between the
 * channel numbers the way the user specifies them in the EDL
 * program and the channel numbers as specified in the header
 * file of the device library.
 *--------------------------------------------------------------*/

static long
me_dio_translate_channel( long channel )
{
    switch ( channel )
    {
        case CHANNEL_CH1 :
            return 0;

        case CHANNEL_CH2 :
            return 1;

        case CHANNEL_CH3 :
            return 2;

        case CHANNEL_CH4 :
            return 3;

        case CHANNEL_CH5 :
            return 4;

        case CHANNEL_CH6 :
            return 5;

        case CHANNEL_CH7 :
            return 6;

        case CHANNEL_CH8 :
            return 7;

        case CHANNEL_CH9 :
            return 8;

        case CHANNEL_CH10 :
            return 9;

        case CHANNEL_CH11 :
            return 10;

        case CHANNEL_CH12 :
            return 11;

        case CHANNEL_CH13 :
            return 12;

        case CHANNEL_CH14 :
            return 13;

        case CHANNEL_CH15 :
            return 14;

        case CHANNEL_CH16 :
            return 15;

        case CHANNEL_CH17 :
            return 16;

        case CHANNEL_CH18 :
            return 17;

        case CHANNEL_CH19 :
            return 18;

        case CHANNEL_CH20 :
            return 19;

        case CHANNEL_CH21 :
            return 20;

        case CHANNEL_CH22 :
            return 21;

        case CHANNEL_CH23 :
            return 22;

        case CHANNEL_CH24 :
            return 23;

        case CHANNEL_CH25 :
            return 24;

        case CHANNEL_CH26 :
            return 25;

        case CHANNEL_CH27 :
            return 26;

        case CHANNEL_CH28 :
            return 27;

        case CHANNEL_CH29 :
            return 28;

        case CHANNEL_CH30 :
            return 29;

        case CHANNEL_CH31 :
            return 30;

        case CHANNEL_CH32 :
            return 31;

        default :
            print( FATAL, "Invalid channel channel, DIO has no channel "
                   "named %s, use either 'CH1' to 'CH32' (depending on"
                   "model of card.\n", Channel_Names[ channel ] );
            THROW( EXCEPTION );
    }

    fsc2_impossible( );

    return -1;        /* we can't end up here */
}



/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
