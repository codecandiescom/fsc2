/*
 *  Copyright (C) 1999-2014 Anton Savitsky / Jens Thoms Toerring
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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "mcn700_2000.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Declaration of exported functions */

int mcn700_2000_init_hook( void );
int mcn700_2000_exp_hook( void );
int mcn700_2000_end_of_exp_hook( void );
 
Var_T * powersupply_name(    Var_T * v );
Var_T * powersupply_voltage( Var_T * v );
Var_T * powersupply_current( Var_T * v );
Var_T * powersupply_command( Var_T * v ); 


/* Locally used functions */

static bool mcn700_2000_init( const char * name );
static double mcn700_2000_set_voltage( double voltage );
static double mcn700_2000_get_voltage( void );
static double mcn700_2000_set_current( double current );
static double mcn700_2000_get_current( void );
static void mcn700_2000_set_voltage_completed( void );
static bool mcn700_2000_command( const char * cmd );
static bool mcn700_2000_talk( const char * cmd,
                              char       * reply,
                              long       * length );
static void mcn700_2000_failure( void );


#define TEST_VOLTAGE        20.0    /* in V */
#define TEST_CURRENT        0.001   /* in A */

#define MAX_VOLTAGE         2000.0  /* in V */
#define MIN_VOLTAGE         0.0     /* in V */
#define VOLTAGE_RESOLUTION  1       /* in V */
#define MAX_CURRENT         0.300   /* in A */
#define MIN_CURRENT         0.000   /* in A */
#define CURRENT_RESOLUTION  1.0e-3  /* in A */


static struct {
    int device;
    double test_volts;
    double test_amps;
} mcn700_2000;


/*------------------------------------*
 * Init hook function for the module.
 *------------------------------------*/

int
mcn700_2000_init_hook( void )
{
    /* Set global variable to indicate that GPIB bus is needed */

    Need_GPIB = SET;

    /* Reset several variables in the structure describing the device */

    mcn700_2000.device     = -1;
    mcn700_2000.test_volts = TEST_VOLTAGE;
    mcn700_2000.test_amps  = TEST_CURRENT;

    return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int
mcn700_2000_exp_hook( void )
{
    /* Initialize the power supply*/

    if ( ! mcn700_2000_init( DEVICE_NAME ) )
    {
        print( FATAL, "Initialization of device failed: %s.\n",
               gpib_last_error( ) );
        THROW( EXCEPTION );
    }

    return 1;
}


/*------------------------------------------------*
 * End of experiment hook function for the module
 *------------------------------------------------*/

int
mcn700_2000_end_of_exp_hook( void )
{
    /* Switch power supply back to local mode */
    mcn700_2000_command("F0\n");
    if ( mcn700_2000.device >= 0 )
        gpib_local( mcn700_2000.device );

    mcn700_2000.device = -1;

    return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var_T *
powersupply_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*-----------------------------------------------------------------*
 * Function sets or returns the voltage. If called with a non-NULL
 * argument it must be the voltage in the range between 0 V and
 * 2000 V (in 1 V steps). If there isn't an argument the voltage
 * at the output gets returned (which may be smaller than the
 * requested voltage due to the current limit).
 * NB: when setting the voltage the program waits until the
 * voltage control is activated (fixed voltage mode)
 *-----------------------------------------------------------------*/

Var_T *
powersupply_voltage( Var_T * v )
{
    double voltage;


    /* If no argument is specified return the actual setting */

    if ( v == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, mcn700_2000.test_volts );
        return vars_push( FLOAT_VAR, mcn700_2000_get_voltage(  ) );
    }

    /* Otherwise the argument must be a voltage between 0 V and 2000 V */

    voltage = get_double( v, "voltage" );

    if ( voltage < MIN_VOLTAGE || voltage > MAX_VOLTAGE )
    {
        print( FATAL, "Voltage of %.0f V is out of valid range "
               "(%.0f to %.0f V).\n", voltage, MIN_VOLTAGE, MAX_VOLTAGE );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == TEST )
    {
        mcn700_2000.test_volts =
                     lrnd( voltage / VOLTAGE_RESOLUTION ) * VOLTAGE_RESOLUTION;
        return vars_push( FLOAT_VAR, voltage );
    }

	mcn700_2000_set_voltage( voltage );
    return vars_push( FLOAT_VAR, mcn700_2000_get_voltage( ) );
}


/*---------------------------------------------------------------*
 * Function sets or returns the current. The argument must be the
 * current in the range between 0 mA and 300 mA (in 1 mA steps).
 * If there's no argument the current at the output is returned
 * (which may be smaller than current that had been set due to
 * the voltage limit).
 *---------------------------------------------------------------*/

Var_T *
powersupply_current( Var_T * v )
{
    double current;


    /* If no argument is specified return the actual setting */

    if ( v == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, mcn700_2000.test_amps );
        return vars_push( FLOAT_VAR, mcn700_2000_get_current(  ) );
    }

    /* Otherwise the argument must be a current between 0 mA and 300 mA */

    current = get_double( v, "current" );

    if ( current < MIN_CURRENT || current > MAX_CURRENT )
    {
        print( FATAL, "Current of %.1f mA is out of valid range "
               "(%.0f to %.0f mA).\n",
			   1000 * current, 1000 * MIN_CURRENT, 1000 * MAX_CURRENT );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == TEST )
    {
        mcn700_2000.test_amps =
                    lrnd( current / CURRENT_RESOLUTION ) * CURRENT_RESOLUTION;
        return vars_push( FLOAT_VAR, mcn700_2000.test_amps );
    }

	mcn700_2000_set_current( current );
    return vars_push( FLOAT_VAR, mcn700_2000_get_current( ) );
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
			cmd = T_strdup( v->val.sptr );
            mcn700_2000_command( translate_escape_sequences( cmd ) );
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


/*--------------------------------------------------------*
 * Internal functions for initialization of power suppply
 *--------------------------------------------------------*/

static bool
mcn700_2000_init( const char * name )
{
    if ( gpib_init_device( name, &mcn700_2000.device ) == FAILURE )
        return FAIL;

    /* Set maximum integration time for measurements, command terminator to
       none and have the device raise EOI, then switch power supply on */

    mcn700_2000_command("S7\n");
    mcn700_2000_command("Y2\n");
    mcn700_2000_command("F1\n");

    return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static double
mcn700_2000_set_voltage( double voltage )
{
    char buffer[ 100 ];


    fsc2_assert( voltage >= MIN_VOLTAGE && voltage <= MAX_VOLTAGE);

    sprintf( buffer, "U%.2f\n", voltage );
    mcn700_2000_command( buffer );
    mcn700_2000_set_voltage_completed(  );

    return voltage;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static double
mcn700_2000_get_voltage( void )
{
    const char *buffer = "N0\n";
    char reply[ 100 ];
    long length = 100;
    

    mcn700_2000_talk( buffer, reply, &length );
    reply[ length - 2 ] = '\0';

    return T_atod( reply );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static double
mcn700_2000_set_current( double current )
{
    char buffer[ 100 ];


    fsc2_assert( current >= MIN_CURRENT && current <= MAX_CURRENT);

    sprintf( buffer, "I%.3f\n", current );
    mcn700_2000_command( buffer );

    return current;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static double
mcn700_2000_get_current( void )
{
    const char *buffer = "N1\n";
    char reply[ 100 ];
    long length = 100;
    

    mcn700_2000_talk( buffer, reply, &length );
    reply[ length - 2 ] = '\0';

    return T_atod( reply );
}


/*-----------------------------------------------------*
 * Function returns once the voltage control is active
 *-----------------------------------------------------*/

void
mcn700_2000_set_voltage_completed( void )
{
    unsigned char stb = 0; 


    while ( 1 )
    {
        fsc2_usleep( 10000, UNSET );

        if ( gpib_serial_poll( mcn700_2000.device, &stb ) == FAILURE )
            mcn700_2000_failure( );

        if ( stb & 0x04 )
            break;

        stop_on_user_request( );
    }
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool
mcn700_2000_command( const char * cmd )
{
    if ( gpib_write( mcn700_2000.device, cmd, strlen( cmd ) ) == FAILURE )
        mcn700_2000_failure( );

    fsc2_usleep( 20000, UNSET );

    return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool
mcn700_2000_talk( const char * cmd,
                  char       * reply,
                  long       * length )
{
    if ( gpib_write( mcn700_2000.device, cmd, strlen( cmd ) ) == FAILURE )
        mcn700_2000_failure( );

    fsc2_usleep( 20000, UNSET );

    if ( gpib_read( mcn700_2000.device, reply, length ) == FAILURE )
        mcn700_2000_failure( );

    fsc2_usleep( 20000, UNSET );

    return OK;
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

static
void mcn700_2000_failure( void )
{
    print( FATAL, "Communication with power supply failed.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
