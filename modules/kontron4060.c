/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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

#include "kontron4060.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Declaration of exported functions */

int kontron4060_init_hook(       void );
int kontron4060_test_hook(       void );
int kontron4060_exp_hook(        void );
int kontron4060_end_of_exp_hook( void );

Var_T * multimeter_name(           Var_T * v );
Var_T * multimeter_mode(           Var_T * v );
Var_T * multimeter_get_data(       Var_T * v );
Var_T * multimeter_ac_measurement( Var_T * v );
Var_T * multimeter_dc_measurement( Var_T * v );
Var_T * multimeter_command(        Var_T * v );


/* Locally used functions */

static bool kontron4060_init( const char * name );

static bool kontron4060_command( const char * cmd );

static void kontron4060_failure( void );


#define KONTRON4060_MODE_VDC      0
#define KONTRON4060_MODE_VAC      1
#define KONTRON4060_MODE_INVALID -1

#define KONTRON4060_TEST_VOLTAGE -0.123


typedef struct Kontron4060 Kontron4060_T;

struct Kontron4060 {
    int device;
    int mode;
};


static Kontron4060_T kontron4060, kontron4060_store;


/*------------------------------------*
 * Init hook function for the module.
 *------------------------------------*/

int
kontron4060_init_hook( void )
{
    /* Set global variable to indicate that GPIB bus is needed */

    Need_GPIB = SET;

    /* Reset several variables in the structure describing the device */

    kontron4060.device = -1;
    kontron4060.mode   = KONTRON4060_MODE_VDC;

    return 1;
}


/*--------------------------------------------*
 * Start of test hook function for the module
 *--------------------------------------------*/

int
kontron4060_test_hook( void )
{
    kontron4060_store = kontron4060;
    return 1;
}


/*--------------------------------------------------*
 * Start of experiment hook function for the module
 *--------------------------------------------------*/

int
kontron4060_exp_hook( void )
{
    kontron4060 = kontron4060_store;

    /* Initialize the multimeter */

    if ( ! kontron4060_init( DEVICE_NAME ) )
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
kontron4060_end_of_exp_hook( void )
{
    /* Switch lock-in back to local mode */

    if ( kontron4060.device >= 0 )
        gpib_local( kontron4060.device );

    kontron4060.device = -1;

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
multimeter_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
multimeter_mode( Var_T *v )
{
    int old_mode = kontron4060.mode;
    char cmd[ ] = "x\n";


    /* Without an argument just return the currently set mode */

    if ( v == NULL )
        return vars_push( INT_VAR, ( long ) kontron4060.mode );

    /* Get and check the mode argument */

    kontron4060.mode = KONTRON4060_MODE_INVALID;

    if ( v->type == STR_VAR )
    {
        if ( ! strcasecmp( v->val.sptr, "VAC" ) )
             kontron4060.mode = KONTRON4060_MODE_VAC;
        else if ( ! strcasecmp( v->val.sptr, "VDC" ) )
             kontron4060.mode = KONTRON4060_MODE_VDC;
    }
    else
        kontron4060.mode = get_strict_long( v, "voltmeter mode" );

    too_many_arguments( v );

    if ( old_mode == kontron4060.mode )
        return vars_push( INT_VAR, ( long ) kontron4060.mode );

    switch ( kontron4060.mode )
    {
        case KONTRON4060_MODE_VDC :
            cmd[ 1 ] = '0';
            break;

        case KONTRON4060_MODE_VAC :
            cmd[ 1 ] = '1';
            break;

        default :
            print( FATAL, "Invalid voltmeter mode, valid are modes are "
                   "\"Vdc\" and \"Vac\".\n" );
            THROW( EXCEPTION );
    }

    if (    FSC2_MODE == EXPERIMENT
         && gpib_write( kontron4060.device, cmd, strlen( cmd ) ) == FAILURE )
        kontron4060_failure( );

    return vars_push( INT_VAR, ( long ) kontron4060.mode );
}


/*------------------------------------------------*
 * Switches the multimeter to AC measurement mode
 *------------------------------------------------*/

Var_T *
multimeter_ac_measurement( Var_T * v  UNUSED_ARG )
{
    if (    FSC2_MODE == EXPERIMENT
         && gpib_write( kontron4060.device, "M1\n", 3 ) == FAILURE )
        kontron4060_failure( );

    kontron4060.mode = KONTRON4060_MODE_VAC;
    return vars_push( INT_VAR, 1L );
}


/*-----------------------------------------------*
 * Switches the multimeter to DC measurement mode
 *-----------------------------------------------*/

Var_T *
multimeter_dc_measurement( Var_T * v  UNUSED_ARG )
{
    if (    FSC2_MODE == EXPERIMENT
         && gpib_write( kontron4060.device, "M0\n", 3 ) == FAILURE )
        kontron4060_failure( );

    kontron4060.mode = KONTRON4060_MODE_VDC;
    return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------*
 * Returns the current voltage from the multimeter
 *------------------------------------------------*/

Var_T *
multimeter_get_data( Var_T * v  UNUSED_ARG )
{
    char reply[ 100 ];
    long length = sizeof reply;
    char buffer[ 100 ];
    double val;


    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, KONTRON4060_TEST_VOLTAGE );

    if (    gpib_write( kontron4060.device, "G\n", 2 ) == FAILURE
         || gpib_read( kontron4060.device, reply, &length ) == FAILURE
         || sscanf( reply, "%s %lf", buffer, &val ) != 2 )
        kontron4060_failure( );

    return vars_push( FLOAT_VAR, val );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
multimeter_command( Var_T * v )
{
    char *cmd = NULL;


    CLOBBER_PROTECT( cmd );

    vars_check( v, STR_VAR );

    if ( FSC2_MODE == EXPERIMENT )
    {
        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            kontron4060_command( cmd );
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


/*--------------------------------------------------*
 *--------------------------------------------------*/

static bool
kontron4060_init( const char * name )
{
    Var_T *v = vars_push( INT_VAR, ( long ) kontron4060.mode );


    if ( gpib_init_device( name, &kontron4060.device ) == FAILURE )
        return FAIL;

    /* Initialize voltmeter to
        1. send data in 7-bit ASCII (B0)
        2. use 6 1/2 digits (D3)
        3. switch filter off (F0)
        4. send data whenever they're ready (H0)
        5. send alphanumeric data (N0)
        6. raise SRQ only on errors (Q0)
        7. automatic scaling (R0)
        8. sample measurement mode (T0)
        9. send only EOI as end of message indicator (U3)
       10. automatic calibration (Y0)
       11. Null function off (Z0)
    */

    if ( gpib_write( kontron4060.device, "B0D3F0H0N0Q0R0T0U3Y0Z0\n", 23 )
         == FAILURE )
        kontron4060_failure( );

    /* Set the measurement mode */

    vars_pop( multimeter_mode( v ) );

    /* Get one value - the first one always seems to be bogus */

    vars_pop( multimeter_get_data( NULL ) );

    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
kontron4060_command( const char * cmd )
{
    if ( gpib_write( kontron4060.device, cmd, strlen( cmd ) ) == FAILURE )
        kontron4060_failure( );

    return OK;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static void
kontron4060_failure( void )
{
    print( FATAL, "Communication with multimeter failed.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
