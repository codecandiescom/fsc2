/*
 *  Copyright (C) 1999-2011 Anton Savitsky / Jens Thoms Toerring
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


#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "thurlby330.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Declaration of exported functions */

int thurlby330_init_hook(       void );
int thurlby330_exp_hook(        void );
int thurlby330_end_of_exp_hook( void );

Var_T * powersupply_name(          Var_T * v );
Var_T * powersupply_damping(       Var_T * v );
Var_T * powersupply_channel_state( Var_T * v );
Var_T * powersupply_voltage(       Var_T * v );
Var_T * powersupply_voltage_limit( Var_T * v );
Var_T * powersupply_current(       Var_T * v );
Var_T * powersupply_current_limit( Var_T * v );
Var_T * powersupply_command(       Var_T * v );


/* Locally used functions */

static bool thurlby330_init( const char * name );

static double thurlby330_set_voltage( long   channel,
                                      double voltage );

static double thurlby330_get_voltage( long channel );

static double thurlby330_get_voltage_limit( long channel );

static double thurlby330_set_current( long   channel,
                                      double current );

static double thurlby330_get_current( long channel );

static double thurlby330_get_current_limit( long channel );

static bool thurlby330_command( const char * cmd );

static bool thurlby330_talk( const char * cmd,
                             char *       reply,
                             long *       length );

static void thurlby330_failure( void );

static long thurlby330_get_channel( Var_T * v );


#define TEST_VOLTAGE        12.12   /* in V */
#define TEST_CURRENT        1.001   /* in A */

#define MAX_VOLTAGE         32.0    /* in V */
#define MIN_VOLTAGE         0.0     /* in V */
#define VOLTAGE_RESOLUTION  1.0e-2  /* in V */
#define MAX_CURRENT         3.1     /* in A */
#define MIN_CURRENT         0.0     /* in A */
#define CURRENT_RESOLUTION  1.0e-3  /* in A */


static struct {
    int device;
} thurlby330;



/*------------------------------------*
 * Init hook function for the module.
 *------------------------------------*/

int
thurlby330_init_hook( void )
{
    /* Set global variable to indicate that GPIB bus is needed */

    Need_GPIB = SET;

    /* Reset several variables in the structure describing the device */

    thurlby330.device = -1;

    return 1;
}


/*--------------------------------------------------*
 * Start of experiment hook function for the module
 *--------------------------------------------------*/

int
thurlby330_exp_hook( void )
{
    /* Initialize the power supply */

    if ( ! thurlby330_init( DEVICE_NAME ) )
    {
        print( FATAL, "Initialization of device failed: %s\n",
               gpib_last_error( ) );
        THROW( EXCEPTION );
    }

    return 1;
}


/*------------------------------------------------*
 * End of experiment hook function for the module
 *------------------------------------------------*/

int
thurlby330_end_of_exp_hook( void )
{
    /* Switch power supply back to local mode */

    if ( thurlby330.device >= 0 )
        gpib_local( thurlby330.device );

    thurlby330.device = -1;

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
powersupply_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*-------------------------------------------------*
 * Switch on/off damping of a power supply channel
 *-------------------------------------------------*/

Var_T *
powersupply_damping( Var_T * v )
{
    long channel;
    long status;
    char buffer[ 100 ];


    /* First argument must be the channel number (1 or 2) */

    channel = thurlby330_get_channel( v );

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing damping status argument.\n" );
        THROW( EXCEPTION );
    }

    /* Second argument must be 0 or "OFF" for OFF or not 0 or "ON" for ON */

    status = get_boolean( v );

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( FLOAT_VAR, status );

    sprintf( buffer, "DAMPING%ld %c\n", channel, status ? '1' : '0' );
    thurlby330_command( buffer );

    return vars_push( INT_VAR, status );
}


/*--------------------------------------*
 * Switch on/off a power supply channel
 *--------------------------------------*/

Var_T *
powersupply_channel_state( Var_T * v )
{
    long channel;
    long status;
    char buffer[ 100 ];


    /* First argument must be the channel number (1 or 2) */

    channel = thurlby330_get_channel( v );

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing channel status argument.\n" );
        THROW( EXCEPTION );
    }

    /* Second argument must be 0 or "OFF" for OFF and not 0 or "ON" for ON */

    status = get_boolean( v );

    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, status );

    sprintf( buffer, "OP%ld %c\n", channel, status ? '1' : '0' );
    thurlby330_command( buffer );

    return vars_push( INT_VAR, status );
}


/*---------------------------------------------------------------*
 * Function sets or returns the voltage at one of the 2 outputs.
 * The first argument must be the output number, either 1 or 2,
 * the second the voltage in the range between 0 V and 32 V (in
 * 10 mV steps). If there isn't a second argument the voltage at
 * the outputs is returned (which may be smaller than the
 * voltage that had been set due to the current limit).
 *---------------------------------------------------------------*/

Var_T *
powersupply_voltage( Var_T * v )
{
    long channel;
    double voltage;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* First argument must be the channel number (1 or 2) */

    channel = thurlby330_get_channel( v );

    /* If no second argument is specified return the actual setting */

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, TEST_VOLTAGE );
        return vars_push( FLOAT_VAR, thurlby330_get_voltage( channel ) );
    }

    /* Second argument must be a voltage between 0 V and 32 V */

    voltage = get_double( v, "voltage" );

    if (    voltage < MIN_VOLTAGE
         || voltage >= MAX_VOLTAGE + 0.5 * VOLTAGE_RESOLUTION )
    {
        print( FATAL, "Voltage of %.1f V is out of valid range "
               "(%.1f to %.1f V).\n", voltage, MIN_VOLTAGE, MAX_VOLTAGE );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, voltage );

    return vars_push( FLOAT_VAR, thurlby330_set_voltage( channel, voltage ) );
}


/*----------------------------------------------------------------*
 * Function sets the voltage limit (which is identical to setting
 * voltage using power-supply_voltage()) when called with two
 * arguments. If called with only one argument the voltage limit
 * setting is returned. The initial voltage limit (i.e. when the
 * module is started is 0 V.
 *----------------------------------------------------------------*/

Var_T *
powersupply_voltage_limit( Var_T * v )
{
    long channel;
    double voltage;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* First argument must be the channel number (1 or 2) */

    channel = thurlby330_get_channel( v );

    /* If there's no second argument return the voltage limit for the
       selected channel */

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, TEST_VOLTAGE );
        return vars_push( FLOAT_VAR, thurlby330_get_voltage_limit( channel ) );
    }

    /* Otherwise set a new current limit (which is actually the same as
       setting a current with power-supply_current()) */

    voltage = get_double( v, "voltage limit" );

    if (    voltage < MIN_VOLTAGE
         || voltage >= MAX_VOLTAGE + 0.5 * VOLTAGE_RESOLUTION )
    {
        print( FATAL, "Voltage limit of %f A is out of valid range "
               "(%.1f to %.1f A).\n", voltage, MIN_VOLTAGE, MAX_VOLTAGE );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, voltage );

    return vars_push( FLOAT_VAR, thurlby330_set_voltage( channel, voltage ) );
}


/*---------------------------------------------------------------*
 * Function sets or returns the current at one of the 2 outputs.
 * The first argument must be the output number, either 1 or 2,
 * the second the current in the range between 0 A and 3 A (in
 * 1 mA steps). If there isn't a second argument the current at
 * the outputs is returned (which may be smaller than the
 * current that had been set due to the voltage limit).
 *---------------------------------------------------------------*/

Var_T *
powersupply_current( Var_T * v )
{
    long channel;
    double current;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* First argument must be the channel number (1 or 2) */

    channel = thurlby330_get_channel( v );

    /* If no second argument is specified return the current output by
     the power supply (not the current limit) */

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, TEST_CURRENT );
        return vars_push( FLOAT_VAR, thurlby330_get_current( channel ) );
    }

    /* Otherwise the second argument must be a current between 0 A and 3 A */

    current = get_double( v, "current" );

    if (    current < MIN_CURRENT
         || current >= MAX_CURRENT + 0.5 * CURRENT_RESOLUTION )
    {
        print( FATAL, "Current of %f A is out of valid range "
               "(%.1f to %.1f A).\n", current, MIN_CURRENT, MAX_CURRENT );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, current );

    return vars_push( FLOAT_VAR, thurlby330_set_current( channel, current ) );
}


/*----------------------------------------------------------------*
 * Function sets the current limit (which is identical to setting
 * current using power-supply_current()) when called with two
 * arguments. If called with only one argument the current limit
 * setting is returned. The initial current limit (i.e. when the
 * module is started is 0 A.
 *----------------------------------------------------------------*/

Var_T *
powersupply_current_limit( Var_T * v )
{
    long channel;
    double current;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* First argument must be the channel number (1 or 2) */

    channel = thurlby330_get_channel( v );

    /* If there's no further variable return the current limit setting */

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, TEST_CURRENT );
        return vars_push( FLOAT_VAR, thurlby330_get_current_limit( channel ) );
    }

    /* Otherwise set a new current limit (which is actually the same as
       setting a current with powersupply_current()) */

    current = get_double( v, "current limit" );

    if (    current < MIN_CURRENT
         || current >= MAX_CURRENT + 0.5 * CURRENT_RESOLUTION )
    {
        print( FATAL, "Current limit of %f A is out of valid range "
               "(%.1f to %.1f A).\n", current, MIN_CURRENT, MAX_CURRENT );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, current );

    return vars_push( FLOAT_VAR, thurlby330_set_current( channel, current ) );
}


/*--------------------------------------------------------------*
 * Function for sending a GPIB command directly to power supply
 *--------------------------------------------------------------*/

Var_T *
powersupply_command( Var_T * v )
{
    char *cmd = NULL;


    CLOBBER_PROTECT( cmd );

    vars_check( v, STR_VAR );

    if ( FSC2_MODE == EXPERIMENT )
    {
        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            thurlby330_command( cmd );
            T_free( cmd );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( cmd );
            RETHROW( );
        }
    }

    return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------*
 * Internal functions for initialization of power-supply
 *-------------------------------------------------------*/

static bool
thurlby330_init( const char * name )
{
    if ( gpib_init_device( name, &thurlby330.device ) == FAILURE )
        return FAIL;

    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double
thurlby330_set_voltage( long   channel,
                        double voltage )
{
    char buffer[ 100 ];


    fsc2_assert( channel == 1 || channel == 2 );
    fsc2_assert(    voltage >= MIN_VOLTAGE
                 && voltage <= MAX_VOLTAGE + 0.5 * VOLTAGE_RESOLUTION );

    sprintf( buffer, "V%ld %.2f\n", channel, voltage );
    thurlby330_command( buffer );
    return voltage;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double
thurlby330_get_voltage( long channel )
{
    char buffer[ 100 ];
    char reply[ 100 ];
    long length = sizeof reply;


    fsc2_assert( channel == 1 || channel == 2 );

    sprintf( buffer, "V%ldO?\n", channel );
    thurlby330_talk( buffer, reply, &length );
    reply[ length - 2 ] = '\0';
    return T_atod( reply );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double
thurlby330_get_voltage_limit( long channel )
{
    char buffer[ 100 ];
    char reply[ 100 ];
    long length = sizeof reply;


    fsc2_assert( channel == 1 || channel == 2 );

    sprintf( buffer, "V%ld?\n", channel );
    thurlby330_talk( buffer, reply, &length );
    reply[ length - 2 ] = '\0';
    return T_atod( reply + 3 );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double
thurlby330_set_current( long   channel,
                        double current )
{
    char buffer[ 100 ];


    fsc2_assert( channel == 1 || channel == 2 );
    fsc2_assert(    current >= MIN_CURRENT
                 && current <= MAX_CURRENT + 0.5 * CURRENT_RESOLUTION );

    sprintf( buffer, "I%ld %.3f\n", channel, current );
    thurlby330_command( buffer );

    return current;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double
thurlby330_get_current( long channel )
{
    char buffer[ 100 ];
    char reply[ 100 ];
    long length = sizeof reply;


    fsc2_assert( channel == 1 || channel == 2 );

    sprintf( buffer, "I%ldO?\n", channel );
    thurlby330_talk( buffer, reply, &length );
    reply[ length - 2 ] = '\0';
    return T_atod( reply );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double
thurlby330_get_current_limit( long channel )
{
    char buffer[ 100 ];
    char reply[ 100 ];
    long length = sizeof reply;


    fsc2_assert( channel == 1 || channel == 2 );

    sprintf( buffer, "I%ld?\n", channel );
    thurlby330_talk( buffer, reply, &length );
    reply[ length - 2 ] = '\0';
    return T_atod( reply + 3 );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
thurlby330_command( const char * cmd )
{
    if ( gpib_write( thurlby330.device, cmd, strlen( cmd ) ) == FAILURE )
        thurlby330_failure( );

    fsc2_usleep( 20000, UNSET );

    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
thurlby330_talk( const char * cmd,
                 char *       reply,
                 long *       length )
{
    if ( gpib_write( thurlby330.device, cmd, strlen( cmd ) ) == FAILURE )
        thurlby330_failure( );

    fsc2_usleep( 20000, UNSET );

    if ( gpib_read( thurlby330.device, reply, length ) == FAILURE )
        thurlby330_failure( );

    fsc2_usleep( 20000, UNSET );

    return OK;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static void
thurlby330_failure( void )
{
    print( FATAL, "Communication with power supply failed.\n" );
    THROW( EXCEPTION );
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static long
thurlby330_get_channel( Var_T * v )
{
    long channel;


    channel = get_long( v, "channel number" );

    if ( channel < CHANNEL_CH1 || channel > CHANNEL_CH2 )
    {
        print( FATAL, "Invalid power supply channel number %ld, valid "
               "channels are 'CH1' and 'CH2'.\n", channel );
        THROW( EXCEPTION );
    }

    return channel + 1;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
