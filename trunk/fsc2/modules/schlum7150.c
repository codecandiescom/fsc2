/*
 *  $Id$
 * 
 *  Copyright (C) 2006-2008 Sergey Weber, Jens Thoms Toerring
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
 */


#include "fsc2_module.h"
#include "gpib_if.h"


/* Include configuration information for the device */

#include "schlum7150.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Declaration of exported functions */

int schlum7150_init_hook( void );
int schlum7150_test_hook( void );
int schlum7150_exp_hook( void );
int schlum7150_end_of_exp_hook( void );

Var_T * multimeter_name( Var_T * v );
Var_T * multimeter_mode( Var_T * v );
Var_T * multimeter_precision( Var_T * v );
Var_T * multimeter_get_data( Var_T * v );
Var_T * multimeter_lock_keyboard( Var_T * v );
Var_T * multimeter_command( Var_T * v );


/* Locally used functions */

static bool schlum7150_init( const char * name );

static void schlum7150_command( const char * cmd );

static void schlum7150_failure( void );


#define SCHLUM7150_MODE_VDC       0
#define SCHLUM7150_MODE_VAC       1
#define SCHLUM7150_MODE_IDC       2
#define SCHLUM7150_MODE_IAC       3
#define SCHLUM7150_MODE_INVALID  -1

#define SCHLUM7150_PREC_35        0
#define SCHLUM7150_PREC_45        1
#define SCHLUM7150_PREC_55        2
#define SCHLUM7150_PREC_65        3
#define SCHLUM7150_PREC_INVALID  -1

#define SCHLUM7150_RANGE_AUTO     0
#define SCHLUM7150_RANGE_02       1
#define SCHLUM7150_RANGE_2        2
#define SCHLUM7150_RANGE_20       3
#define SCHLUM7150_RANGE_200      4
#define SCHLUM7150_RANGE_2000     5
#define SCHLUM7150_RANGE_INVALID -1

#define SCHLUM7150_TIME_OUT      15.0         /* 15 s timeout */

#define SCHLUM7150_TEST_VOLTAGE  -0.123
#define SCHLUM7150_TEST_CURRENT  -1.001
#define SCHLUM7150_TEST_MODE     SCHLUM7150_MODE_VDC  /* also default mode */
#define SCHLUM7150_TEST_PREC     SCHLUM7150_PREC_55   /* also default prec. */


typedef struct Schlum7150 Schlum7150_T;

struct Schlum7150 {
    int device;
    int mode;
    int prec;
    int range;
};

Schlum7150_T schlum7150, schlum7150_store;


/*------------------------------------*
 * Init hook function for the module.
 *------------------------------------*/

int
schlum7150_init_hook( void )
{
    /* Set global variable to indicate that GPIB bus is needed */

    Need_GPIB = SET;

    /* Initialize variables in the structure describing the device */

    schlum7150.device = -1;
    schlum7150.mode   = SCHLUM7150_TEST_MODE;
    schlum7150.mode   = SCHLUM7150_TEST_PREC;
    schlum7150.range  = SCHLUM7150_RANGE_INVALID;

    return 1;
}


/*--------------------------------------------*
 * Start of test hook function for the module
 *--------------------------------------------*/

int
schlum7150_test_hook( void )
{
    /* Store the state of the device as it is before the test run */

    schlum7150_store = schlum7150;
    return 1;
}


/*--------------------------------------------------*
 * Start of experiment hook function for the module
 *--------------------------------------------------*/

int
schlum7150_exp_hook( void )
{
    /* Restore the structure for the multimeter to the way it was after
       the PREPARATIONS section had been processed */

    schlum7150 = schlum7150_store;

    /* Initialize the multimeter */

    if ( ! schlum7150_init( DEVICE_NAME ) )
    {
        print( FATAL, "Initialization of device failed: %s\n",
               gpib_error_msg );
        THROW( EXCEPTION );
    }

    return 1;
}


/*------------------------------------------------*
 * End of experiment hook function for the module
 *------------------------------------------------*/

int
schlum7150_end_of_exp_hook( void )
{
    /* Switch multimeter back to local mode */

    if ( schlum7150.device >= 0 )
        gpib_local( schlum7150.device );

    schlum7150.device = -1;

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
multimeter_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------*
 * Multimeter mode selection
 *---------------------------*/

Var_T *
multimeter_mode( Var_T * v )
{
    char cmd[ ] = "Mx\n";
    int old_mode = schlum7150.mode;


    /* Without an argument just return the currently set mode */

    if ( v == NULL )
        return vars_push( INT_VAR, ( long ) schlum7150.mode );

    /* Get and check the mode argument */ 

    schlum7150.mode = SCHLUM7150_MODE_INVALID;

    if ( v->type == STR_VAR )
    {
        if ( ! strcasecmp( v->val.sptr, "VDC" ) )
            schlum7150.mode = SCHLUM7150_MODE_VDC;
        else if ( ! strcasecmp( v->val.sptr, "VAC" ) )
            schlum7150.mode = SCHLUM7150_MODE_VAC;
        else if ( ! strcasecmp( v->val.sptr, "IDC" ) )
            schlum7150.mode = SCHLUM7150_MODE_IDC;
        else if ( ! strcasecmp( v->val.sptr, "IAC" ) )
            schlum7150.mode = SCHLUM7150_MODE_IAC;
    }
    else
        schlum7150.mode = ( int ) get_strict_long( v, "multimeter mode" ); 

    too_many_arguments( v );

    /* Nothing to be done if the mode didn't change */

    if ( old_mode == schlum7150.mode )
        return vars_push( INT_VAR, ( long ) schlum7150.mode );

    switch ( schlum7150.mode )
    {
        case SCHLUM7150_MODE_VDC :
            cmd[ 2 ] = '0';
            break;

        case SCHLUM7150_MODE_VAC :
            cmd[ 1 ] = '1';
            break;

        case SCHLUM7150_MODE_IDC :
            cmd[ 1 ] = '3';
            break;

        case SCHLUM7150_MODE_IAC :
            cmd[ 1 ] = '4';
            break;

        default :
            print( FATAL, "Invalid multimeter mode, valid are modes are "
                   "\"Idc\", \"Iac\", \"Vdc\" or \"Vac\".\n" );
            THROW( EXCEPTION );
    }

    /* Set the new mode when we're doing an experiment */

    if (    FSC2_MODE == EXPERIMENT
         && gpib_write( schlum7150.device, cmd, strlen( cmd ) ) == FAILURE )
        schlum7150_failure( );

    return vars_push( INT_VAR, ( long ) schlum7150.mode );
}


/*---------------------------------*
 * Multimeter precision selection
 *---------------------------------*/

Var_T *
multimeter_precision( Var_T * v )
{
    char cmd[ ] = "Ix\n";
    int old_prec = schlum7150.prec;


    /* Without an argument just return the currently set precision */

    if ( v == NULL )
        return vars_push( INT_VAR, ( long ) schlum7150.prec );

    /* Get and check the precision argument */ 

    schlum7150.prec = SCHLUM7150_PREC_INVALID;

    if ( v->type == STR_VAR )
    {
        if ( ! strcmp( v->val.sptr, "3.5" ) )
            schlum7150.prec = SCHLUM7150_PREC_35;
        else if ( ! strcmp( v->val.sptr, "4.5" ) )
            schlum7150.prec = SCHLUM7150_PREC_45;
        else if ( ! strcmp( v->val.sptr, "5.5" ) )
            schlum7150.prec = SCHLUM7150_PREC_55;
        else if ( ! strcmp( v->val.sptr, "5.5" ) )
            schlum7150.prec = SCHLUM7150_PREC_65;
    }
    else
        schlum7150.prec = ( int ) get_strict_long( v, "multimeter precision" );

    too_many_arguments( v );

    /* Nothing to be done if the precision didn't change */

    if ( old_prec == schlum7150.prec )
        return vars_push( INT_VAR, ( long ) schlum7150.prec );

    switch ( schlum7150.prec )
    {
        case SCHLUM7150_PREC_35 :
            cmd[ 1 ] = '0';
            break;

        case SCHLUM7150_PREC_45 :
            cmd[ 1 ] = '1';
            break;

        case SCHLUM7150_PREC_55 :
            cmd[ 1 ] = '3';
            break;

        case SCHLUM7150_PREC_65 :
            cmd[ 1 ] = '4';
            break;

        default :
            print( FATAL, "Invalid multimeter precision, valid are "
                   "\"3.5\",\"4.5\",\"5.5\" or \"6.5\" digits.\n" );
            THROW( EXCEPTION );
    }

    /* Set the new precision if we're doing an experiment */

    if (    FSC2_MODE == EXPERIMENT
         && gpib_write( schlum7150.device, cmd, strlen( cmd ) ) == FAILURE )
        schlum7150_failure( );

    return vars_push( INT_VAR, ( long ) schlum7150.prec );
}


/*---------------------------------------------------------*
 * Returns the result of a measurement from the multimeter
 *---------------------------------------------------------*/

Var_T *
multimeter_get_data( Var_T * v )
{
    char cmd[ ] = "Rx\n";
    char reply[ 100 ];
    long length = 100;
    double start_time;
    unsigned char stb = 0;
    int old_range = schlum7150.range;


    /* If there's an argument it must be the precision, if there's none
       the precision is set to autoscale mode */

    if ( v != NULL )
    {
        vars_check( v, STR_VAR );

        /* Get and check the range argument */ 

        if ( ! strcasecmp( v->val.sptr, "0.2V" ) )
            schlum7150.range = SCHLUM7150_RANGE_02;
        else if ( ! strcasecmp( v->val.sptr, "2V" ) )
            schlum7150.range = SCHLUM7150_RANGE_2;
        else if ( ! strcasecmp( v->val.sptr, "20V" ) )
            schlum7150.range = SCHLUM7150_RANGE_20;
        else if ( ! strcasecmp( v->val.sptr, "200V" ) )
            schlum7150.range = SCHLUM7150_RANGE_200;
        else if ( ! strcasecmp( v->val.sptr, "2000V" ) )
            schlum7150.range = SCHLUM7150_RANGE_2000;
        else
        {
            print(  FATAL,"Invalid range argument, valid are "
                    "\"0.2V\",\"2V\",\"20V\",\"200V\" or \"2000V\" "
                    "or no argument (for autorange).\n" );
            THROW( EXCEPTION );
        }
    } 
    else
        schlum7150.range = SCHLUM7150_RANGE_AUTO;

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        if (    schlum7150.mode == SCHLUM7150_MODE_VDC
             || schlum7150.mode == SCHLUM7150_MODE_VAC )
            return vars_push( FLOAT_VAR, SCHLUM7150_TEST_VOLTAGE );
        else
            return vars_push( FLOAT_VAR, SCHLUM7150_TEST_CURRENT );
    }

    /* Set the range (but only if it changed since the last invocation) */

    if ( old_range != schlum7150.range )
    {
        cmd[ 1 ] = ( char ) ( schlum7150.range + '0' );
        if ( gpib_write( schlum7150.device, cmd, strlen( cmd ) ) == FAILURE ) 
            schlum7150_failure( );
    }

    /* Set OFF Track mode */

    if ( gpib_write( schlum7150.device, "T0\n", 3 ) == FAILURE ) 
        schlum7150_failure( );  

    /* Start a 'single-shot' measurement and wait for the result, looping
       until either the value is ready or a timeout is detected */

    if ( gpib_write( schlum7150.device, "G\n", 2 ) == FAILURE) 
        schlum7150_failure( );

    fsc2_usleep ( 20000, UNSET );
    start_time = time( 0 );

    while ( 1 )
    {
        if ( difftime( time( 0 ), start_time ) > SCHLUM7150_TIME_OUT )
        {
            print( FATAL, "Timeout (%.1f s).\n", SCHLUM7150_TIME_OUT );
            THROW( EXCEPTION );
        }

        if ( gpib_serial_poll( schlum7150.device, &stb ) == FAILURE )
            schlum7150_failure( );

        if ( stb & 0x10 )
        {
            if ( gpib_read( schlum7150.device, reply, &length ) == FAILURE )
                schlum7150_failure( );

            reply[ length - 1 ] = '\0';

            if ( ! isspace( reply[ 10 ] ) )
                print( WARN, "Multimeter overloaded, please adjust range.\n" );

            break;
        }

        fsc2_usleep( 10000, UNSET );
    }

    /* Switch back to ON Track mode */

    if ( gpib_write( schlum7150.device, "T1\n", 3 ) == FAILURE ) 
        schlum7150_failure( );  

    return vars_push( FLOAT_VAR, T_atod( reply ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
multimeter_lock_keyboard( Var_T * v )
{
    bool lock;
    char cmd[ ] = "Kx\n";


    if ( v == NULL )
        lock = SET;
    else
    {
        lock = get_boolean( v );
        too_many_arguments( v );
    }

    cmd[ 1 ] = lock ? '1' : '0';

    if (    FSC2_MODE == EXPERIMENT
         && gpib_write( schlum7150.device, "K1\n", 3 ) == FAILURE )
        schlum7150_failure( );

    return vars_push( INT_VAR, lock ? 1L : 0L );
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
            schlum7150_command( cmd );
            T_free( cmd );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            if ( cmd != NULL )
                T_free( cmd );
            RETHROW( );
        }
    }

    return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static bool
schlum7150_init( const char * name )
{
    char cmd[ ] = "C0D0I3J0K1M3N0Q0R0T0U7Y0Z0\n";


    if ( gpib_init_device( name, &schlum7150.device ) == FAILURE )
        return FAIL;

    /* Initialize multimeter to
        1. Normal operating mode (C0)
        2. Display on (D0)
        3. Integration Time: 400ms (I3)
        4. No response to Parallel Poll (J0)
        5. LOCAL key disabled (K1)
        6. Selected Mode: Vdc (M0)
        7. Numeric output with literals(N0)  important for overload control
        8. raise SRQ only on errors (Q0)
        9. automatic scaling (R0)
       10. sample measurement mode (T0)
       11. Output delimeter: carriage returns (U7)
       12. Timed drift-corrects enabled (Y0)
       13. Null function off (Z0)
    */

    switch ( schlum7150.mode )
    {
        case SCHLUM7150_MODE_VDC :
            cmd[ 11 ] = '0';                                /* M0 */
            break;

        case SCHLUM7150_MODE_VAC :
            cmd[ 11 ] = '1';                                /* M1 */
            break;

        case SCHLUM7150_MODE_IDC :
            cmd[ 11 ] = '3';                                /* M3 */
            break;

        case SCHLUM7150_MODE_IAC :
            cmd[ 11 ] = '4';                                /* M4 */
            break;
    }

    switch ( schlum7150.prec )
    {
        case SCHLUM7150_PREC_35 :
            cmd[ 5 ] = '0';                                /* I0 */
            break;

        case SCHLUM7150_PREC_45 :
            cmd[ 5 ] = '1';                                /* I1 */
            break;

        case SCHLUM7150_PREC_55 :
            cmd[ 5 ] = '3';                                /* I3 */
            break;

        case SCHLUM7150_PREC_65 :
            cmd[ 5 ] = '4';                                /* I4 */
            break;
    }

    if ( gpib_write( schlum7150.device, cmd, strlen( cmd ) ) == FAILURE )
        schlum7150_failure( );

    vars_pop( multimeter_get_data( NULL ) );

    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
schlum7150_command( const char * cmd )
{
    if ( gpib_write( schlum7150.device, cmd, strlen( cmd ) ) == FAILURE )
        schlum7150_failure( );
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static void
schlum7150_failure( void )
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
