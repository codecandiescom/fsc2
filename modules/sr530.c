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


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* This device needs '\n' (0x0A) as EOS - set this correctly in 'gpib.conf' */
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "sr530.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Here values are defined that get returned by the driver in the test run
   when the lock-in can't be accessed - these values must really be
   reasonable ! */

#define SR530_TEST_ADC_VOLTAGE   0.0
#define SR530_TEST_SENSITIVITY   0.5
#define SR530_TEST_TIME_CONSTANT 0.1
#define SR530_TEST_PHASE         0.0
#define SR530_TEST_REF_FREQUENCY 5.0e3


/* Declaration of exported functions */

int sr530_init_hook(       void );
int sr530_test_hook(       void );
int sr530_exp_hook(        void );
int sr530_end_of_exp_hook( void );

Var_T * lockin_name(          Var_T * v );
Var_T * lockin_get_data(      Var_T * v );
Var_T * lockin_get_adc_data(  Var_T * v );
Var_T * lockin_sensitivity(   Var_T * v );
Var_T * lockin_time_constant( Var_T * v );
Var_T * lockin_phase(         Var_T * v );
Var_T * lockin_ref_freq(      Var_T * v );
Var_T * lockin_dac_voltage(   Var_T * v );
Var_T * lockin_is_overload(   Var_T * v );
Var_T * lockin_is_locked(     Var_T * v );
Var_T * lockin_has_reference( Var_T * v );
Var_T * lockin_lock_keyboard( Var_T * v );
Var_T * lockin_command(       Var_T * v );


/* Exported symbols (used by W-band power supply driver) */

int first_DAC_port = 5;
int last_DAC_port = 6;


/* Global variables used only in this file */

struct SR530 {
    int device;
    int sens_index;
    bool sens_warn;
    double phase;
    bool is_phase;
    int tc_index;
    double dac_voltage[ 2 ];
};


static struct SR530 sr530, sr530_stored;


#define UNDEF_SENS_INDEX -1
#define UNDEF_TC_INDEX   -1

/* Lists of valid sensitivity and time constant settings (the last three
   entries in the sensitivity list are only usable when the EXPAND button
   is switched on!) */

static double sens_list[ ] = { 5.0e-1, 2.0e-1, 1.0e-1, 5.0e-2, 2.0e-2,
                               1.0e-2, 5.0e-3, 2.0e-3, 1.0e-3, 5.0e-4,
                               2.0e-4, 1.0e-4, 5.0e-5, 2.0e-5, 1.0e-5,
                               5.0e-6, 2.0e-6, 1.0e-6, 5.0e-7, 2.0e-7,
                               1.0e-7, 5.0e-8, 2.0e-8, 1.0e-8 };

/* List of all available time constants */

static double tc_list[ ] = { 1.0e-3, 3.0e-3, 1.0e-2, 3.0e-2, 1.0e-1, 3.0e-1,
                             1.0, 3.0, 10.0, 30.0, 100.0 };


/* Declaration of all functions used only in this file */

static double get_single_channel_data( Var_T * v );
static bool sr530_init( const char * name );
static double sr530_get_data( int channel );
static double sr530_get_adc_data( long channel );
static double sr530_get_sens( void );
static void sr530_set_sens( int sens_index );
static double sr530_get_tc( void );
static void sr530_set_tc( int tc_index );
static double sr530_get_phase( void );
static double sr530_set_phase( double phase );
static double sr530_get_ref_freq( void );
static double sr530_set_dac_voltage( long   channel,
                                     double voltage );
static bool sr530_get_overload( void );
static bool sr530_is_locked( void );
static bool sr530_has_reference( void );
static void sr530_lock_state( bool lock );
static bool sr530_command( const char * cmd );
static bool sr530_talk( const char * cmd,
                        char       * reply,
                        long       * length );
static void sr530_failure( void );



/*------------------------------------*
 * Init hook function for the module.
 *------------------------------------*/

int
sr530_init_hook( void )
{
    int i;


    /* Set global variable to indicate that GPIB bus is needed */

    Need_GPIB = SET;

    /* Reset several variables in the structure describing the device */

    sr530.device = -1;

    sr530.sens_index = UNDEF_SENS_INDEX;
    sr530.sens_warn = UNSET;
    sr530.is_phase = UNSET;
    sr530.tc_index = UNDEF_TC_INDEX;

    for ( i = 0; i < 2; i++ )
        sr530.dac_voltage[ i ]  = 0.0;

    return 1;
}


/*-----------------------------------*
 * Test hook function for the module
 *-----------------------------------*/

int
sr530_test_hook( void )
{
    sr530_stored = sr530;
    return 1;
}


/*--------------------------------------------------*
 * Start of experiment hook function for the module
 *--------------------------------------------------*/

int
sr530_exp_hook( void )
{
    /* Reset the device structure to the state it had before the test run */

    sr530 = sr530_stored;

    /* Initialize the lock-in */

    if ( ! sr530_init( DEVICE_NAME ) )
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
sr530_end_of_exp_hook( void )
{
    /* Switch lock-in back to local mode */

    if ( sr530.device >= 0 )
    {
        gpib_write( sr530.device, "I0\n", 3 );
        gpib_local( sr530.device );
    }

    sr530.device = -1;
    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
lockin_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*-------------------------------------------------------------------------*
 * Function returns the lock-in voltage(s) in V, with the range depending
 * on the current sensitivity setting. It may be called with no parameter,
 * in which case the voltage at channel 1 is returned, with one parameter,
 * which designates the channel number (1 or 2), which returns the voltage
 * at the selected channel, or two parameters, both designating channels,
 * which will return an array of the voltages at both the channels.
 *-------------------------------------------------------------------------*/

Var_T *
lockin_get_data( Var_T * v )
{
    double val[ 2 ];

    if ( v == NULL )
        return vars_push( FLOAT_VAR, get_single_channel_data( v ) );

    val[ 0 ] = get_single_channel_data( v );

    if ( ( v = vars_pop( v ) ) == NULL )
        return vars_push( FLOAT_VAR, val[ 0 ] );

    val[ 1 ] = get_single_channel_data( v );

    too_many_arguments( v );

    return vars_push( FLOAT_ARR, val, 2L );
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static double
get_single_channel_data( Var_T * v )
{
    long channel;


    if ( v == NULL )
        channel = 1;
    else
        channel = ( v == NULL ) ?
                      1 : get_long( v, "channel number" );

    if ( channel != 1 && channel != 2 )
    {
        print( FATAL, "Invalid channel number %ld, valid channels are "
               "1 or 2.\n", channel );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == TEST )               /* return dummy value in test run */
        return 0.0;

    return sr530_get_data( ( int ) channel );
}


/*-----------------------------------------------------------------*
 * Function returns the voltage at one or more of the 4 ADC ports
 * on the backside of the lock-in amplifier. The argument(s) must
 * be integers between 1 and 4.
 * Returned values are in the interval [ -10.24V, +10.24V ].
 *-----------------------------------------------------------------*/

Var_T *
lockin_get_adc_data( Var_T * v )
{
    long port;


    port = get_long( v, "ADC number" );

    if ( port < 1 || port > 4 )
    {
        print( FATAL, "Invalid ADC channel number (%ld), valid channels are "
               "in the range between 1 and 4.\n", port );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == TEST )               /* return dummy value in test run */
        return vars_push( FLOAT_VAR, SR530_TEST_ADC_VOLTAGE );

    return vars_push( FLOAT_VAR, sr530_get_adc_data( port ) );
}


/*-------------------------------------------------------------------------*
 * Returns or sets sensitivity of the lock-in amplifier. If called with no
 * argument the current sensitivity is returned, otherwise the sensitivity
 * is set to the argument. By using the EXPAND button the sensitivity can
 * be increased by a factor of 10.
 *-------------------------------------------------------------------------*/

Var_T *
lockin_sensitivity( Var_T * v )
{
    double sens;
    int sens_index = UNDEF_SENS_INDEX;
    unsigned int i;


    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                no_query_possible( );

            case TEST :
                return vars_push( FLOAT_VAR,
                                  sr530.sens_index == UNDEF_SENS_INDEX ?
                                  SR530_TEST_SENSITIVITY :
                                  sens_list[ sr530.sens_index ] );

            case EXPERIMENT :
                return vars_push( FLOAT_VAR, sr530_get_sens( ) );
        }

    sens = get_double( v, "sensitivity" );

    if ( sens < 0.0 )
    {
        print( FATAL, "Invalid negative sensitivity.\n" );
        THROW( EXCEPTION );
    }

    /* We try to match the sensitivity passed to the function by checking if
       it fits in between two of the valid values and setting it to the nearer
       one and, if this doesn't work, we set it to the minimum or maximum
       value, depending on the size of the argument. If the value does not fit
       within 1 percent, we utter a warning message (but only once). */

    for ( i = 0; i < NUM_ELEMS( sens_list ) - 1; i++ )
        if ( sens <= sens_list[ i ] && sens >= sens_list[ i + 1 ] )
        {
            if ( sens_list[ i ] / sens < sens / sens_list[ i + 1 ] )
                sens_index = i;
            else
                sens_index = i + 1;
            break;
        }

    if (    sens_index == UNDEF_SENS_INDEX
         && sens >= sens_list[ NUM_ELEMS( sens_list ) - 1 ] / 1.01
         && sens < sens_list[ NUM_ELEMS( sens_list ) - 1 ] )
        sens_index = NUM_ELEMS( sens_list ) - 1;

    if ( sens_index >= 0
         &&  fabs( sens - sens_list[ sens_index ] ) > sens * 1.0e-2
         && ! sr530.sens_warn )
    {
        if ( sens >= 1.0e-3 )
            print( WARN, "Can't set sensitivity to %.0lf mV, using %.0lf mV "
                   "instead.\n",
                   sens * 1.0e3, sens_list[ sens_index ] * 1.0e3 );
        else if ( sens >= 1.0e-6 )
            print( WARN, "Can't set sensitivity to %.0lf uV, using %.0lf uV "
                   "instead.\n",
                   sens * 1.0e6, sens_list[ sens_index ] * 1.0e6 );
        else
            print( WARN, "Can't set sensitivity to %.0lf nV, using %.0lf nV "
                   "instead.\n",
                   sens * 1.0e9, sens_list[ sens_index ] * 1.0e9 );
        sr530.sens_warn = SET;
    }

    if ( sens_index == UNDEF_SENS_INDEX )                 /* not found yet ? */
    {
        if ( sens > sens_list[ 0 ] )
            sens_index = 0;
        else
            sens_index = NUM_ELEMS( sens_list ) - 1;

        if ( ! sr530.sens_warn )                      /* no warn message yet */
        {
            if ( sens >= 1.0e-3 )
                print( WARN, "Sensitivity of %.0lf mV is too low, using %.0lf "
                       "mV instead.\n",
                       sens * 1.0e3, sens_list[ sens_index ] * 1.0e3 );
            else
                print( WARN, "Sensitivity of %.0lf nV is too high, using "
                       "%.0lf nV instead.\n",
                       sens * 1.0e9, sens_list[ sens_index ] * 1.0e9 );
            sr530.sens_warn = SET;
        }
    }

    too_many_arguments( v );

    sr530.sens_index = sens_index;

    if ( FSC2_MODE == EXPERIMENT )
        sr530_set_sens( sens_index );

    return vars_push( FLOAT_VAR, sens_list[ sens_index ] );
}


/*------------------------------------------------------------------------*
 * Returns or sets the time constant of the lock-in amplifier. If called
 * without an argument the time constant is returned (in secs). If called
 * with an argumet the time constant is set to this value.
 *------------------------------------------------------------------------*/

Var_T *
lockin_time_constant( Var_T * v )
{
    double tc;
    int tc_index = UNDEF_TC_INDEX;
    unsigned int i;


    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                no_query_possible( );

            case TEST :
                return vars_push( FLOAT_VAR, sr530.tc_index == UNDEF_TC_INDEX ?
                                  SR530_TEST_TIME_CONSTANT :
                                  tc_list[ sr530.tc_index ] );

            case EXPERIMENT :
                return vars_push( FLOAT_VAR, sr530_get_tc( ) );
        }

    tc = get_double( v, "time constant" );

    if ( tc < 0.0 )
    {
        print( FATAL, "Invalid negative time constant.\n" );
        THROW( EXCEPTION );
    }

    /* We try to match the time constant passed to the function by checking if
       it fits in between two of the valid values and setting it to the nearer
       one and, if this doesn't work, we set it to the minimum or maximum
       value, depending on the size of the argument. If the value does not fit
       within 1 percent, we utter a warning message (but only once). */

    for ( i = 0; i < NUM_ELEMS( tc_list ) - 1; i++ )
        if ( tc >= tc_list[ i ] && tc <= tc_list[ i + 1 ] )
        {
            if ( tc / tc_list[ i ] < tc_list[ i + 1 ] / tc )
                tc_index = i;
            else
                tc_index = i + 1;
            break;
        }

    if ( tc_index >= 0
         && fabs( tc - tc_list[ tc_index ] ) > tc * 1.0e-2 )
    {
        if ( tc >= 1.0 )
            print( WARN, "Can't set time constant to %g s, using %.0lf s "
                   "instead.\n", tc, tc_list[ tc_index ] );
        else
            print( WARN, "Can't set time constant to %g ms, using %.0lf ms "
                   "instead.\n", tc * 1.0e3, tc_list[ tc_index ] * 1.0e3 );
    }

    if ( tc_index == UNDEF_SENS_INDEX )                   /* not found yet ? */
    {
        if ( tc < tc_list[ 0 ] )
            tc_index = 0;
        else
            tc_index = NUM_ELEMS( tc_list ) - 1;

        if ( tc >= 1.0 )
            print( WARN, "Time constant of %0.lf s is too large, using "
                   "%.0lf s instead.\n", tc, tc_list[ tc_index ] );
        else if ( tc >= 1.0e-3 )
            print( WARN, "Time constant of %.0lf ms is too short, using "
                   "%.0lf ms instead.\n",
                   tc * 1.0e3, tc_list[ tc_index ] * 1.0e3 );
        else
            print( WARN, "Time constant of %lf ms is too short, using "
                   "%.0lf ms instead.\n",
                   tc * 1.0e3, tc_list[ tc_index ] * 1.0e3 );
    }

    too_many_arguments( v );

    sr530.tc_index = tc_index;

    if ( FSC2_MODE == EXPERIMENT )
        sr530_set_tc( tc_index );

    return vars_push( FLOAT_VAR, tc_list[ tc_index ] );
}


/*-----------------------------------------------------------------*
 * Returns or sets the phase of the lock-in amplifier. If called
 * without an argument the current phase setting is returned (in
 * degree between 0 and 359). Otherwise the phase is set to value
 * passed to the function (after conversion to 0-359 degree range)
 * and the value the phase is set to is returned.
 *-----------------------------------------------------------------*/

Var_T *
lockin_phase( Var_T * v )
{
    double phase;


    /* Without an argument just return current phase settting */

    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                no_query_possible( );

            case TEST :
                return vars_push( FLOAT_VAR, sr530.is_phase ?
                                  sr530.phase : SR530_TEST_PHASE );

            case EXPERIMENT :
                return vars_push( FLOAT_VAR, sr530_get_phase( ) );
        }

    /* Otherwise set phase to value passed to the function */

    phase = get_double( v, "phase" );

    while ( phase >= 360.0 )    /* convert to 0-359 degree range */
        phase -= 360.0;

    if ( phase < 0.0 )
    {
        phase *= -1.0;
        while ( phase >= 360.0 )    /* convert to 0-359 degree range */
            phase -= 360.0;
        phase = 360.0 - phase;
    }

    too_many_arguments( v );

    sr530.phase    = phase;
    sr530.is_phase = SET;

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( FLOAT_VAR, phase );

    return vars_push( FLOAT_VAR, sr530_set_phase( phase ) );
}


/*------------------------------------------------------------*
 * Function returns the reference frequency, can only be used
 * for queries.
 *------------------------------------------------------------*/

Var_T *
lockin_ref_freq( Var_T * v )
{
    if ( v != NULL )
    {
        print( FATAL, "Reference frequency cannot be set for this model.\n" );
        THROW( EXCEPTION );
    }

    switch ( FSC2_MODE )
    {
        case PREPARATION :
            no_query_possible( );

        case TEST :
            return vars_push( FLOAT_VAR, SR530_TEST_REF_FREQUENCY );
    }

    return vars_push( FLOAT_VAR, sr530_get_ref_freq( ) );
}


/*-----------------------------------------------------------*
 * Function sets or returns the voltage at one of the 2 DAC
 * ports on the backside of the lock-in amplifier. The first
 * argument must be the channel number, either 5 or 6, the
 * second the voltage in the range between -10.24 V and
 * +10.24 V. If there isn't a second argument the set DAC
 * voltage is returned (which is initially set to 0 V).
 *-----------------------------------------------------------*/

Var_T *
lockin_dac_voltage( Var_T * v )
{
    long channel;
    double voltage;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* First argument must be the channel number (5 or 6) */

    channel = get_long( v, "DAC channel" );

    if ( channel < first_DAC_port || channel > last_DAC_port )
    {
        print( FATAL, "Invalid lock-in DAC channel number %ld, valid channels "
               "are in the range from %d to %d.\n",
                channel, first_DAC_port, last_DAC_port );
        THROW( EXCEPTION );
    }

    /* If no second argument is specified return the current DAC setting */

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        if ( FSC2_MODE == PREPARATION )
            no_query_possible( );

        return vars_push( FLOAT_VAR,
                          sr530.dac_voltage[ channel - first_DAC_port ] );
    }

    /* Second argument must be a voltage between -10.24 V and +10.24 V */

    voltage = get_double( v, "DAC voltage" );

    if ( fabs( voltage ) > 10.24 )
    {
        print( FATAL, "DAC voltage of %f V is out of valid range "
               "(+/-10.24 V).\n", voltage );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    sr530.dac_voltage[ channel - first_DAC_port ] = voltage;

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( FLOAT_VAR, voltage );

    return vars_push( FLOAT_VAR, sr530_set_dac_voltage( channel, voltage ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
lockin_is_overload( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, 0L );

    return vars_push( INT_VAR, ( long ) sr530_get_overload( ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
lockin_is_locked( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, 1L );

    return vars_push( INT_VAR, ( long ) sr530_is_locked( ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
lockin_has_reference( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, 1L );

    return vars_push( INT_VAR, ( long ) sr530_has_reference( ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
lockin_lock_keyboard( Var_T * v )
{
    bool lock;


    if ( v == NULL )
        lock = SET;
    else
    {
        lock = get_boolean( v );
        too_many_arguments( v );
    }

    if ( FSC2_MODE == EXPERIMENT )
        sr530_lock_state( lock );

    return vars_push( INT_VAR, lock ? 1L : 0L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
lockin_command( Var_T * v )
{
    vars_check( v, STR_VAR );

    if ( FSC2_MODE == EXPERIMENT )
    {
        char * volatile cmd = NULL;

        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            sr530_command( cmd );
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



/******************************************************/
/* The following functions are only used internally ! */
/******************************************************/


/*------------------------------------------------------------------*
 * Function initialises the Stanford lock-in amplifier and tests if
 * it can be accessed by asking it to send its status byte.
 *------------------------------------------------------------------*/

static bool
sr530_init( const char * name )
{
    char buffer[ 20 ];
    long length = sizeof buffer;
    int i;
    unsigned long stb = 0;


    if ( gpib_init_device( name, &sr530.device ) == FAILURE )
    {
        sr530.device = -1;
        return FAIL;
    }

    /* Ask lock-in to send status byte and test if it does */

    if (    gpib_write( sr530.device, "Y\n", 2 ) == FAILURE
         || gpib_read( sr530.device, buffer, &length ) == FAILURE )
        return FAIL;

    /* Check that there's a reference input and the internal reference is
       locked to it, if not print a warning (note: status byte gets returned
       as a ASCII coded decimal and not a binary value as Ivo Alxneit pointed
       out) */

    buffer[ length - 2 ] = '\0';
    for ( i = 0; buffer[ i ] != '\0'; i++ )
    {
        if ( ! isdigit( ( unsigned char ) buffer[ i ] ) )
        {
            print( FATAL, "Device sent unexpected data for status byte.\n" );
            return FAIL;
        }

        stb = 10 * stb + buffer[ i ] - '0';
    }

    if ( stb > 0xFF )
    {
        print( FATAL, "Device sent unexpected data for status byte.\n" );
        return FAIL;
    }
    if ( stb & 4 )
        print( SEVERE, "No reference input detected.\n" );
    if ( stb & 8 )
        print( SEVERE, "Reference oszillator not locked to reference "
               "input.\n" );

    /* Lock the keyboard */

    sr530_command( "I1\n" );

    /* If sensitivity, time constant or phase were set in one of the
       preparation sections only the value was stored and we have to do the
       actual setting now because the lock-in could not be accessed before
       Finally set the DAC output voltages to a defined value (default 0 V).*/

    if ( sr530.sens_index != UNDEF_SENS_INDEX )
        sr530_set_sens( sr530.sens_index );
    if ( sr530.is_phase == SET )
        sr530_set_phase( sr530.phase );
    if ( sr530.tc_index != UNDEF_TC_INDEX )
        sr530_set_tc( sr530.tc_index );
    for ( i = 0; i < 2; i++ )
        sr530_set_dac_voltage( i + first_DAC_port, sr530.dac_voltage[ i ] );

    return OK;
}


/*------------------------------------------------------------*
 * lockin_data() returns the measured voltage of the lock-in.
 *------------------------------------------------------------*/

static double
sr530_get_data( int channel )
{
    char cmd[ 5 ] = "Q*\n";
    char buffer[ 20 ];
    long length = sizeof buffer;


    fsc2_assert( channel == 1 || channel == 2 );

    cmd[ 1 ] = ( char ) ( channel + '0' );
    sr530_talk( cmd, buffer, &length );
    buffer[ length - 2 ] = '\0';
    return T_atod( buffer );
}


/*----------------------------------------------------------*
 * lockin_get_adc() returns the value of the voltage at one
 * of the 4 ADC ports.
 * -> Number of the ADC channel (1-4)
 *----------------------------------------------------------*/

static double
sr530_get_adc_data( long channel )
{
    char buffer[ 16 ] = "X*\n";
    long length = sizeof buffer;


    buffer[ 1 ] = ( char ) channel + '0';
    sr530_talk( buffer, buffer, &length );
    buffer[ length - 2 ] = '\0';
    return T_atod( buffer );
}


/*-----------------------------------------------------------------------*
 * Function determines the sensitivity setting of the lock-in amplifier.
 *-----------------------------------------------------------------------*/

static double
sr530_get_sens( void )
{
    char buffer[ 10 ];
    long length = sizeof buffer;
    double sens;

    /* Ask lock-in for the sensitivity setting */

    sr530_talk( "G\n", buffer, &length );
    buffer[ length - 2 ] = '\0';
    sens = sens_list[ NUM_ELEMS( sens_list ) - T_atol( buffer ) ];

    /* Check if EXPAND is switched on - this increases the sensitivity
       by a factor of 10 */

    length = sizeof buffer;
    sr530_talk( "E1\n", buffer, &length );
    if ( buffer[ 0 ] == '1' )
        sens *= 0.1;

    return sens;
}


/*----------------------------------------------------------------------*
 * Function sets the sensitivity of the lock-in amplifier to one of the
 * valid values. The parameter can be in the range from 0 to 23,  where
 * 0 is 0.5 V and 23 is 10 nV - these and the other values in between
 * are listed in the global array 'sens_list' at the start of the file.
 * To achieve sensitivities below 100 nV EXPAND has to switched on.
 *----------------------------------------------------------------------*/

static void
sr530_set_sens( int sens_index )
{
    char buffer[ 10 ];


    /* Coding of sensitivity commands work just the other way round as
       in the list of sensitivities 'sens_list', i.e. 1 stands for the
       highest sensitivity (10nV) and 24 for the lowest (500mV) */

    sens_index = NUM_ELEMS( sens_list ) - sens_index;

    /* For sensitivities lower than 100 nV EXPAND has to be switched on
       (for both channels) otherwise it got to be switched off */

    if ( sens_index <= 3 )
    {
        sr530_command( "E1,1\n" );
        sr530_command( "E2,1\n" );
        sens_index += 3;
    }
    else
    {
        sr530_command( "E1,0\n" );
        sr530_command( "E2,0\n" );
    }

    /* Now set the sensitivity */

    sprintf( buffer, "G%d\n", sens_index );
    sr530_command( buffer );
}


/*----------------------------------------------------------------------*
 * Function returns the current time constant of the lock-in amplifier.
 * See also the global array 'tc_list' with the possible time constants
 * at the start of the file.
 *----------------------------------------------------------------------*/

static double
sr530_get_tc( void )
{
    char buffer[10];
    long length = sizeof buffer;


    sr530_talk( "T1\n", buffer, &length );
    buffer[ length - 2 ] = '\0';
    return tc_list[ T_atol( buffer ) - 1 ];
}


/*-------------------------------------------------------------------------*
 * Function sets the time constant (plus the post time constant) to one
 * of the valid values. The parameter can be in the range from 0 to 10,
 * where 0 is 1 ms and 10 is 100 s - these and the other values in between
 * are listed in the global array 'tc_list' (cf. start of file)
 *-------------------------------------------------------------------------*/

static void
sr530_set_tc( int tc_index )
{
    char buffer[ 10 ];


    sprintf( buffer, "T1,%d\n", tc_index + 1 );
    sr530_command( buffer );

    /* Also set the POST time constant where 'T2,0' switches it off, 'T2,1'
       sets it to 100ms and 'T1,2' to 1s */

    if ( tc_index < 4 )
        sr530_command( "T2,0\n" );
    else if ( tc_index >= 4 && tc_index < 6 )
        sr530_command( "T2,1\n" );
    else if ( tc_index >= 6 )
        sr530_command( "T2,2\n" );
}


/*-----------------------------------------------------------*
 * Function returns the current phase setting of the lock-in
 * amplifier (in degree between 0 and 359).
 *-----------------------------------------------------------*/

static double
sr530_get_phase( void )
{
    char buffer[ 20 ];
    long length = sizeof buffer;
    double phase;


    sr530_talk( "P\n", buffer, &length );
    buffer[ length - 2 ] = '\0';
    phase = T_atod( buffer );

    while ( phase >= 360.0 )    /* convert to 0-359 degree range */
        phase -= 360.0;

    while ( phase < 0.0 )
        phase += 360.0;

    return phase;
}


/*---------------------------------------------------------------*
 * Functions sets the phase to a value between 0 and 360 degree.
 *---------------------------------------------------------------*/

static double
sr530_set_phase( double phase )
{
    char buffer[ 20 ];


    sprintf( buffer, "P%.2f\n", phase );
    sr530_command( buffer );
    return phase;
}


/*------------------------------------------*
 * Function returns the reference frequency
 *------------------------------------------*/

static double
sr530_get_ref_freq( void )
{
    char buffer[ 50 ];
    long length = sizeof buffer;


    sr530_talk( "F\n", buffer, &length );
    buffer[ length - 2 ] = '\0';
    return T_atod( buffer );
}


/*----------------------------------------*
 * Functions sets the DAC output voltage.
 *----------------------------------------*/

static double
sr530_set_dac_voltage( long   channel,
                       double voltage )
{
    char buffer[ 30 ];


    /* Just some more sanity checks, should already been done by calling
       function... */

    fsc2_assert( channel >= first_DAC_port || channel <= last_DAC_port );

    if ( voltage > 10.24 )
        voltage = 10.24;
    if ( voltage < -10.24 )
        voltage = -10.24;

    sprintf( buffer, "X%1ld,%f\n", channel, voltage );
    sr530_command( buffer );
    return voltage;
}


/*-------------------------------------------------------*
 * Function returns if the lock-in detected an overload.
 *-------------------------------------------------------*/

static bool
sr530_get_overload( void )
{
    char buffer[ 20 ];
    long length = sizeof buffer;


    sr530_talk( "Y4\n", buffer, &length );
    return buffer[ 0 ] == '1';
}


/*----------------------------------------------------------*
 * Function returns if the lock-in locked to the reference.
 *----------------------------------------------------------*/

static bool
sr530_is_locked( void )
{
    char buffer[ 20 ];
    long length = sizeof buffer;


    sr530_talk( "Y3\n", buffer, &length );
    return buffer[ 0 ] == '0';
}


/*--------------------------------------------------*
 * Function returns if the lock-in has a reference. 
 *--------------------------------------------------*/

static bool
sr530_has_reference( void )
{
    char buffer[ 20 ];
    long length = sizeof buffer;


    sr530_talk( "Y2\n", buffer, &length );
    return buffer[ 0 ] == '0';
}


/*-----------------------------------------*
 * Function locks or unlocks the keyboard. 
 *-----------------------------------------*/

static void
sr530_lock_state( bool lock )
{
    char cmd[ 100 ];


    sprintf( cmd, "I%c\n", lock ? '2' : '0' );
    sr530_command( cmd );
}


/*-----------------------------------------*
 * Function sends a command to the device. 
 *-----------------------------------------*/

static bool
sr530_command( const char * cmd )
{
    if ( gpib_write( sr530.device, cmd, strlen( cmd ) ) == FAILURE )
        sr530_failure( );
    return OK;
}


/*---------------------------------------------------------------*
 * Function sends a command to the device and returns the reply.
 *---------------------------------------------------------------*/

static bool
sr530_talk( const char * cmd,
            char       * reply,
            long       * length )
{
    if (    gpib_write( sr530.device, cmd, strlen( cmd ) ) == FAILURE
         || gpib_read( sr530.device, reply, length ) == FAILURE )
        sr530_failure( );
    return OK;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static void
sr530_failure( void )
{
    print( FATAL, "Can't access the lock-in amplifier.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
