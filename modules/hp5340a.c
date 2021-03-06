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


/*
 *   The HP5340A is a very old device, it's from 1973, i.e. even pre-dating
 *   the GPIB standard. Its programming capabilities are rather limited,
 *   especially one can't find out about the settings of the device and
 *   one either has to set all parameters via the program or none at all.
 *   Thus, to keep things simple, all settings must be made via the front
 *   panel and the only function that is supported is querying the measured
 *   frequency.
 *
 *   The device is set to a state where it measures new frequencies with an
 *   internal sample rate (which can be adjusted via a knob at the front
 *   panel) and to output data only when it becomes addressed as talker.
 */


#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "hp5340a.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


int hp5340a_init_hook(       void );
int hp5340a_exp_hook(        void );
int hp5340a_end_of_exp_hook( void );


Var_T * freq_counter_name(    Var_T * v );
Var_T * freq_counter_measure( Var_T * v );
Var_T * freq_counter_command( Var_T * v );


static bool hp5340a_init( const char * name );

static double hp5340a_get_freq( void );

static bool hp5340a_command( const char * cmd );


static struct {
    int device;
} hp5340a;


/* Define the frequency that the driver returns during the test run */

#define HP5340A_TEST_FREQUENCY  9.2e9



/**********************************************************/
/*                                                        */
/*                  hook functions                        */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
hp5340a_init_hook( void )
{
    /* Set global variable to indicate that GPIB bus is needed */

    Need_GPIB = SET;

    hp5340a.device = -1;
    return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
hp5340a_exp_hook( void )
{
    if ( ! hp5340a_init( device_name ) )
    {
        print( FATAL, "Initialization of device failed: %s.\n",
               gpib_last_error( ) );
        THROW( EXCEPTION );
    }

    return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
hp5340a_end_of_exp_hook( void )
{
    /* Do a reset and switch device to local mode */

    /* Do a reset and switch device to local mode */

    if ( hp5340a.device != -1 )
    {
        gpib_write( hp5340a.device, "NH", 2 );

        gpib_local( hp5340a.device );
        hp5340a.device = -1;
    }

    return 1;
}


/**************************************/
/*                                    */
/*          EDL functions             */
/*                                    */
/**************************************/

/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
freq_counter_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
freq_counter_measure( Var_T * v )
{
    too_many_arguments( v );

    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, HP5340A_TEST_FREQUENCY );

    return vars_push( FLOAT_VAR, hp5340a_get_freq( ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
freq_counter_command( Var_T * v )
{
    vars_check( v, STR_VAR );

    if ( FSC2_MODE == EXPERIMENT )
    {
        char * volatile cmd = NULL;

        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            hp5340a_command( cmd );
            T_free( cmd );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( cmd );
            RETHROW;
        }
    }

    return vars_push( INT_VAR, 1L );
}



/**************************************************/
/*                                                */
/*       Internal (non-exported) functions        */
/*                                                */
/**************************************************/

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static bool
hp5340a_init( const char * name )
{
    if ( gpib_init_device( name, &hp5340a.device ) == FAILURE )
        return FAIL;

    /* Tell device to use internal sample rate and to output data only if
       addressed as talker(don't lock the keyboard, the user is supposed
       to do settings at the front panel) */

    if ( gpib_write( hp5340a.device, "J", 1 ) == FAILURE )
        return FAIL;

    if ( gpib_write( hp5340a.device, "L", 1 ) == FAILURE )
        return FAIL;

    return OK;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static double
hp5340a_get_freq( void )
{
    char buf[ 16 ];
    long len;


 restart_get_freq:
    len = sizeof buf;
    if (    gpib_read( hp5340a.device, buf, &len ) == FAILURE
         || len != sizeof buf )
    {
        print( FATAL, "Communication with device failed.\n" );
        THROW( EXCEPTION );
    }

    /* This is a dirty hack: sometimes the first byte to be read goes AWOL
       and in this case we read simply read single bytes until the next
       byte to be read should be the start of a message.. */

    if ( buf[ len - 1 ] == 'L' )
    {
        do
        {
            len = 1;
            if (    gpib_read( hp5340a.device, buf, &len ) == FAILURE
                 || len != 1 )
            {
                print( FATAL, "Communication with device failed.\n" );
                THROW( EXCEPTION );
            }
        } while ( *buf != '\n' );
        goto restart_get_freq;
    }

    if ( buf[ 1 ] != ' ' )
    {
        print( FATAL, "Device detected display overflow.\n" );
        THROW( EXCEPTION );
    }

    if ( buf[ 2 ] != ' ' )
    {
        print( FATAL, "Communication with device failed.\n" );
        THROW( EXCEPTION );
    }

    buf[ 14 ] = '\0';

    return T_atod( buf + 3 );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
hp5340a_command( const char * cmd )
{
    if ( gpib_write( hp5340a.device, cmd, strlen( cmd ) ) == FAILURE )
    {
        print( FATAL, "Communication with device failed.\n" );
        THROW( EXCEPTION );
    }

    return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
