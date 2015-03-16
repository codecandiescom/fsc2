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


#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "itc503.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define C2K_OFFSET   273.16         /* offset between Celsius and Kelvin */


#define MAX_RETRIES            10

#define SAMPLE_CHANNEL_1        1
#define SAMPLE_CHANNEL_2        2
#define SAMPLE_CHANNEL_3        3

#define DEFAULT_SAMPLE_CHANNEL  SAMPLE_CHANNEL_1
#define DEFAULT_HEATER_SENSOR   SAMPLE_CHANNEL_1;


enum {
    LOCAL_AND_LOCKED,
    REMOTE_AND_LOCKED,
    LOCAL_AND_UNLOCKED,
    REMOTE_AND_UNLOCKED
};


#define MAX_TEMP        1667.7    /* max. temperature in Kelvin */

#define TEST_SETPOINT   123.4

enum {
    STATE_MANUAL,
    STATE_HEATER_AUTO,
    STATE_GAS_AUTO,
    STATE_AUTO,
    STATE_AutoGFS
};


#define TEST_STATE           STATE_MANUAL

#define MAX_HEATER_POWER     40.0    /* 40 V */


#define TEST_HEATER_POWER    99.9
#define TEST_GAS_FLOW        99.9


#define MAX_INTEGRAL_TIME     8400   /* 140 minutes */
#define MAX_DERIVATIVE_TIME  16380   /* 273 minutes */ 


int itc503_init_hook(       void );
int itc503_exp_hook(        void );
int itc503_end_of_exp_hook( void );


Var_T * temp_contr_name(               Var_T * v );
Var_T * temp_contr_temperature(        Var_T * v );
Var_T * temp_contr_sample_channel(     Var_T * v );
Var_T * temp_contr_heater_sensor(      Var_T * v );
Var_T * temp_contr_setpoint(           Var_T * v );
Var_T * temp_contr_state(              Var_T * v );
Var_T * temp_contr_heater_power_limit( Var_T * v );
Var_T * temp_contr_heater_power(       Var_T * v );
Var_T * temp_contr_gas_flow(           Var_T * v );
Var_T * temp_contr_sensor_unit(        Var_T * v );
Var_T * temp_contr_lock_keyboard(      Var_T * v );
Var_T * temp_contr_command(            Var_T * v );


static bool itc503_init( const char * name );
static int itc503_get_state( void );
static void itc503_set_state( int state );
static int itc503_get_heater_sensor( void );
static void itc503_set_heater_sensor( int sensor );
static double itc503_get_setpoint( void );
static void itc503_set_setpoint( double setpoint );
static double itc503_sens_data( long channel );
static void itc503_set_heater_power_limit( double limit );
static double itc503_get_heater_power(void );
static void itc503_set_heater_power( double hp );
static double itc503_get_gas_flow( void );
static void itc503_set_gas_flow( double gf );
static void itc503_lock( int state );
#if 0
static void itc503_auto_pid_mode( bool state );
static bool itc503_get_auto_pid_state( void );
static void itc503_set_proportional_band( double prop_bnd );
static double itc503_get_proportional_band( void );
static void itc503_set_integral_time( double int_time );
static double itc503_get_integral_time( void );
static void itc503_set_derivative_time( double deriv_time );
static double itc503_get_derivative_time( void );
#endif
static bool itc503_command( const char * cmd );
static long itc503_talk( const char * message,
                         char *       reply,
                         long         length );
static void itc503_gpib_failure( void );


static struct {
    int    device;
    int    lock_state;
    int    state;
    int    sample_channel;
    double setpoint;
    int    heater_sensor;
    double heater_power;
    double gas_flow;
} itc503;



/**********************************************************/
/*                                                        */
/*                  Hook functions                        */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
itc503_init_hook( void )
{
    Need_GPIB = SET;

    itc503.device           = -1;
    itc503.lock_state       = REMOTE_AND_LOCKED;
    itc503.state            = TEST_STATE;
    itc503.setpoint         = TEST_SETPOINT;
    itc503.sample_channel   = DEFAULT_SAMPLE_CHANNEL;
    itc503.heater_sensor    = DEFAULT_HEATER_SENSOR;
    itc503.heater_power     = TEST_HEATER_POWER;
    itc503.gas_flow         = TEST_GAS_FLOW;

    return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
itc503_exp_hook( void )
{
    if ( ! itc503_init( DEVICE_NAME ) )
    {
        print( FATAL, "Initialization of device failed.\n" );
        THROW( EXCEPTION );
    }

    itc503.sample_channel = SAMPLE_CHANNEL_1;
    return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
itc503_end_of_exp_hook( void )
{
    itc503_lock( LOCAL_AND_UNLOCKED );
    gpib_local( itc503.device );
    return 1;
}


/**********************************************************/
/*                                                        */
/*          EDL functions                                 */
/*                                                        */
/**********************************************************/

/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
temp_contr_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------*
 * Returns temperature reading from controller
 *---------------------------------------------*/

Var_T *
temp_contr_temperature( Var_T * v )
{
    long channel;


    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, 123.45 );

    if ( v == NULL )
        return vars_push( FLOAT_VAR,
                          itc503_sens_data( itc503.sample_channel ) );

    vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

    if ( v->type & ( INT_VAR | FLOAT_VAR ) )
    {
        channel = get_long( v, "channel number" );

        if ( channel < SAMPLE_CHANNEL_1 && channel > SAMPLE_CHANNEL_3 )
        {
            print( FATAL, "Invalid sample channel number (%ld).\n", channel );
            THROW( EXCEPTION );
        }
    }
    else
    {
        if (    strlen( v->val.sptr ) != 1
             || (    *v->val.sptr != 'A'
                  && *v->val.sptr != 'B'
                  && *v->val.sptr != 'C' ) )
        {
            print( FATAL, "Invalid sample channel (\"%s\").\n", v->val.sptr );
            THROW( EXCEPTION );
        }

        channel = ( long ) ( *v->val.sptr - 'A' + 1 );
    }

    if ( channel > NUM_SAMPLE_CHANNELS )
    {
        print( FATAL, "Device module is configured to use not more than %d "
               "sample channels.\n", NUM_SAMPLE_CHANNELS );
        THROW( EXCEPTION );
    }

    itc503.sample_channel = channel - 1;

    return vars_push( FLOAT_VAR,
                      itc503_sens_data( itc503.sample_channel ) );
}


/*------------------------------------------------------------------*
 * Sets or returns the currently used sample channel for returning
 * measured temperatures. The function returns either 1, 2 or 3 for
 * channel A, B or C and accepts 1, 2, 3 or the strings "A","B" or
 * "C" as input arguments). Please note that the number of sample
 * channels that can be used is a compile time constant that can be
 * changed in the configuration file.
 *------------------------------------------------------------------*/

Var_T *
temp_contr_sample_channel( Var_T * v )
{
    long channel;


    if ( v == NULL )
        return vars_push( INT_VAR, ( long ) itc503.sample_channel );

    vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

    if ( v->type & ( INT_VAR | FLOAT_VAR ) )
    {
        channel = get_long( v, "channel number" );

        if ( channel < SAMPLE_CHANNEL_1 && channel > SAMPLE_CHANNEL_3  )
        {
            print( FATAL, "Invalid sample channel number (%ld).\n", channel );
            THROW( EXCEPTION );
        }
    }
    else
    {
        if (    strlen( v->val.sptr ) != 1
             || (    *v->val.sptr != 'A'
                  && *v->val.sptr != 'B'
                  && *v->val.sptr != 'C' ) )
        {
            print( FATAL, "Invalid sample channel (\"%s\").\n", v->val.sptr );
            THROW( EXCEPTION );
        }

        channel = ( long ) ( *v->val.sptr - 'A' + 1 );
    }

    if ( channel > NUM_SAMPLE_CHANNELS )
    {
        print( FATAL, "Device module is configured to use not more than %d "
               "sample channels.\n", NUM_SAMPLE_CHANNELS );
        THROW( EXCEPTION );
    }

    itc503.sample_channel = channel;

    return vars_push( INT_VAR, itc503.sample_channel );
}


/*-------------------------------------------------------*
 * Sets or returns the currently used channel for active
 * temerature control. The (function returns either 1, 2
 * or 3 for channel A, B or C and accepts 1, 2, 3 or the
 * strings "A","B" or "C" as input arguments). Please
 * note that the number of sample channels that can be
 * used is a compile time constant that can be changed
 * in the configuration file.
 *-------------------------------------------------------*/

Var_T *
temp_contr_heater_sensor( Var_T * v )
{
    long sensor;


    if ( v == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( INT_VAR, ( long ) itc503.heater_sensor );
        return vars_push( INT_VAR, ( long ) itc503_get_heater_sensor );
    }

    if ( v->type & ( INT_VAR | FLOAT_VAR ) )
    {
        sensor = get_long( v, "heater sensor" );

        if ( sensor < SAMPLE_CHANNEL_1 && sensor > SAMPLE_CHANNEL_3 )
        {
            print( FATAL, "Invalid heater sensor number (%ld).\n", sensor );
            THROW( EXCEPTION );
        }
    }
    else
    {
        if (    strlen( v->val.sptr ) != 1
             || (    *v->val.sptr != 'A'
                  && *v->val.sptr != 'B'
                  && *v->val.sptr != 'C' ) )
        {
            print( FATAL, "Invalid heater sensor \"%s\".\n", v->val.sptr );
            THROW( EXCEPTION );
        }

        sensor = ( long ) ( *v->val.sptr - 'A' + 1 );
    }

    if ( sensor > NUM_SAMPLE_CHANNELS )
    {
        print( FATAL, "Device module is configured to use not more than %d "
               "heater sensors channels.\n", NUM_SAMPLE_CHANNELS );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == TEST )
        itc503.heater_sensor = sensor;
    else
        itc503_set_heater_sensor( sensor );

    return vars_push( INT_VAR, ( long ) sensor );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
temp_contr_setpoint( Var_T * v )
{
    double sp;


    if ( v == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, itc503.setpoint );
        return vars_push( FLOAT_VAR, itc503_get_setpoint( ) );
    }

    sp = get_double( v, "setpoint" );

    too_many_arguments( v );

    if ( sp < 0.0 || sp > MAX_TEMP )
    {
        print( FATAL, "Requested setpoint out of range\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        itc503.setpoint = sp;
        return vars_push( FLOAT_VAR, sp );
    }

    itc503_set_setpoint( sp );
    return vars_push( FLOAT_VAR, itc503_get_setpoint( ) );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
temp_contr_state( Var_T * v )
{
    long state;


    if ( v == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( INT_VAR, ( long ) TEST_STATE );
        return vars_push( INT_VAR, ( long ) itc503_get_state( ) );
    }

    if ( v->type == STR_VAR )
    {
        if ( ! strcmp( v->val.sptr, "MANUAL" ) )
            state = STATE_MANUAL;
        else if ( ! strcmp( v->val.sptr, "HEATER_AUTO" ) )
            state = STATE_HEATER_AUTO;
        else if ( ! strcmp( v->val.sptr, "GAS_AUTO" ) )
            state = STATE_GAS_AUTO;
        else if ( ! strcmp( v->val.sptr, "AUTO" ) )
            state = STATE_AUTO;
        else
        {
            print( FATAL, "Invalid argument '%s'.\n", v->val.sptr );
            THROW( EXCEPTION );
        }
    }
    else
    {
        state = get_long( v, "state" );
        if ( state < STATE_MANUAL || state > STATE_AUTO )
        {
            print( FATAL, "Invald argument '%ld\n", state );
            THROW( EXCEPTION );
        }
    }

    too_many_arguments( v );

    if ( FSC2_MODE == TEST )
        itc503.state = state;
    else
        itc503_set_state( state );

    return vars_push( INT_VAR, state );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
temp_contr_heater_power_limit( Var_T * v )
{
    double limit;


    if ( v == NULL )
    {
        print( FATAL, "Heater power linit can't be queried.\n" );
        THROW( EXCEPTION );
    }

    limit = get_double( v, "heater power limit" );

    too_many_arguments( v );

    if ( itc503.state == STATE_MANUAL )
    {
        print( SEVERE, "Heater power limit can only be set while device "
               "is in manual state.\n" );
        return vars_push( FLOAT_VAR, -1.0 );
    }

    if ( limit < 0.0 || limit > MAX_HEATER_POWER + 0.04 )
    {
        print( FATAL, "Heater power limit of %.1f V out of range, must be "
               "between 0 and %.1f V.\n", limit, MAX_HEATER_POWER );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        itc503_set_heater_power_limit( limit );

    return vars_push( FLOAT_VAR, 0.1 * lrnd( 10.0 * limit ) );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
temp_contr_heater_power( Var_T * v )
{
    double hp;


    if ( v == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, itc503.heater_power );
        return vars_push( FLOAT_VAR, itc503_get_heater_power( ) );
    }

    hp = get_double( v, "heater power" );

    too_many_arguments( v );

    if ( itc503.state == STATE_HEATER_AUTO || itc503.state == STATE_AUTO )
    {
        print( SEVERE, "Can't change heater power while heater power is "
               "controlled by the device.\n" );
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, itc503.heater_power );
        return vars_push( FLOAT_VAR, itc503_get_heater_power( ) );
    }

    if ( hp < 0 || lrnd( 10 * hp ) > 999 )
    {
        print( FATAL, "Heater power of %.1f %% out of range, must be between "
               "0 and 99.9 %%.\n", hp );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == TEST )
    {
        itc503.heater_power = 0.1 * lrnd( 10 * hp );
        return vars_push( FLOAT_VAR, itc503.heater_power );
    }

    itc503_set_heater_power( hp );
    return vars_push( FLOAT_VAR, itc503_get_heater_power( ) );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
temp_contr_gas_flow( Var_T * v )
{
    double gf;


    if ( v == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, itc503.gas_flow );
        return vars_push( FLOAT_VAR, itc503_get_gas_flow( ) );
    }

    gf = get_double( v, "gas flow" );

    too_many_arguments( v );

    if ( itc503.state == STATE_GAS_AUTO || itc503.state == STATE_AUTO )
    {
        print( SEVERE, "Can't change gas flow while gas flow is "
               "controlled by the device.\n" );
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, itc503.heater_power );
        return vars_push( FLOAT_VAR, itc503_get_heater_power( ) );
    }

    if ( gf < 0 || lrnd( 10 * gf ) > 999 )
    {
        print( FATAL, "Gas flow of %.1f %% out of range, must be between "
               "0 and 99.9 %%.\n", gf );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == TEST )
    {
        itc503.gas_flow = 0.1 * lrnd( 10 * gf );
        return vars_push( FLOAT_VAR, itc503.gas_flow );
    }

    itc503_set_gas_flow( gf );
    return vars_push( FLOAT_VAR, itc503_get_gas_flow( ) );
}


/*-----------------------------------------------------*
 * If called with a zero argument the keyboard becomes
 * unlocked during an experiment.
 *-----------------------------------------------------*/

Var_T *
temp_contr_lock_keyboard( Var_T * v )
{
    int lock;


    if ( v == NULL )
        lock = REMOTE_AND_LOCKED;
    else
    {
        lock = get_boolean( v ) ? REMOTE_AND_LOCKED : REMOTE_AND_UNLOCKED;
        too_many_arguments( v );

        if ( FSC2_MODE == EXPERIMENT )
            itc503_lock( lock );
    }

    return vars_push( INT_VAR, lock == REMOTE_AND_LOCKED ? 1L : 0L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
temp_contr_command( Var_T * v )
{
    char *cmd = NULL;


    CLOBBER_PROTECT( cmd );

    vars_check( v, STR_VAR );

    if ( FSC2_MODE == EXPERIMENT )
    {
        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            itc503_command( cmd );
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


/**********************************************************/
/*                                                        */
/*       Internal (non-exported) functions                */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static bool
itc503_init( const char * name )
{
    char buf[ 10 ];


    /* Initialize GPIB communication with the temperature controller */

    if ( gpib_init_device( name, &itc503.device ) == FAILURE )
        return FAIL;

    /* Set end of EOS character to '\r' */

    if ( gpib_write( itc503.device, "Q0\r", 3 ) == FAILURE )
        return FAIL;

    /* Switch device per default to remote state with local lockout */

    if ( itc503_talk( "C1\r", buf, sizeof buf ) != 2 )
        return FAIL;

    itc503.state = itc503_get_state( );

    itc503.heater_sensor = itc503_get_heater_sensor( );

    return OK;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static int
itc503_get_state( void )
{
    char buf[ 50 ];
    int state;


    if (    itc503_talk( "X\r", buf, sizeof buf ) < 4
         || buf[ 2 ] != 'A' )
    {
        print( FATAL, "Device returns invalid state data.\n");
        THROW( EXCEPTION );
    }

    state = T_atoi( buf + 3 );
    if ( state < STATE_MANUAL || state > STATE_AUTO + 4 )
    {
        print( FATAL, "Device returns invalid state data.\n");
        THROW( EXCEPTION );
    }

    return state;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static void
itc503_set_state( int state )
{
    char cmd[ ] = "A*\r";
    char buf[ 10 ];


    fsc2_assert( state >= STATE_MANUAL || state <= STATE_AUTO );

    itc503.state = itc503_get_state( );

    if ( itc503.state > STATE_AUTO && state >= STATE_GAS_AUTO )
    {
        print( FATAL, "Can't set state to GAS AUTO while in AutoGFS phase.\n" );
        THROW( EXCEPTION );
    }

    cmd[ 1 ] = state + '0';
    if ( itc503_talk( cmd, buf, sizeof buf ) != 2 )
    {
        print( FATAL, "Failure to set state.\n" );
        THROW( EXCEPTION );
    }
}


/*--------------------------------------------*
 *--------------------------------------------*/

static int
itc503_get_heater_sensor( void )
{
    char buf[ 50 ];
    int sensor;


    if (    itc503_talk( "X\r", buf, sizeof buf ) != 14
         || buf[ 9 ] != 'H' )
    {
        print( FATAL, "Device returns invalid heater sensor data.\n");
        THROW( EXCEPTION );
    }

    sensor = T_atoi( buf + 10 );

    if ( sensor < SAMPLE_CHANNEL_1 || sensor > SAMPLE_CHANNEL_3 )
    {
        print( FATAL, "Device returns invalid heater sensor data.\n");
        THROW( EXCEPTION );
    }

    if ( sensor > NUM_SAMPLE_CHANNELS )
    {
        print( FATAL, "Device returns heater sensor %d which is higher than "
               "what has been set as the upper sensor in the configuration "
               "of the device (%d).\n", sensor, NUM_SAMPLE_CHANNELS );
        THROW( EXCEPTION );
    }

    return sensor;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static void
itc503_set_heater_sensor( int sensor )
{
    char cmd[ 20 ];
    char buf[ 20 ];


    fsc2_assert( sensor >= SAMPLE_CHANNEL_1 && sensor <= NUM_SAMPLE_CHANNELS );

    sprintf( cmd, "H%d\r", sensor );
    if ( itc503_talk( cmd, buf, sizeof buf ) != 2 )
    {
        print( FATAL, "Failed to set heater sensor.\n" );
        THROW( EXCEPTION );
    }
}


/*--------------------------------------------*
 * Returns the measured temperature in Kelvin
 *--------------------------------------------*/

static double
itc503_sens_data( long channel )
{
    char buf[ 50 ];
    long len;
    double temp;
    char cmd[ ] = "R*\r";


    fsc2_assert(    channel >= SAMPLE_CHANNEL_1
                 && channel <= NUM_SAMPLE_CHANNELS );

    cmd[ 1 ] = channel + '0';
    len = itc503_talk( cmd, buf, sizeof buf );

    if (    len < 2
         || (    buf[ 1 ] != '-'
              && buf[ 1 ] != '+'
              && ! isdigit( ( unsigned char ) buf[ 1 ] ) ) )
    {
        print( FATAL, "Error reading temperature.\n" );
        THROW( EXCEPTION );
    }

    sscanf( buf + 1, "%lf", &temp );

    /* If the first character is a '+' or '-' the sensor is returning
       temperatures in degree Celsius, otherwise in Kelvin */

    if ( ! isdigit( ( unsigned char ) buf[ 1 ] ) )
         temp += C2K_OFFSET;

    return temp;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static double
itc503_get_setpoint( void )
{
    char buf[ 50 ];
    long len;
    double setpoint;


    len = itc503_talk( "R0\r", buf, sizeof buf );

    if (    len < 2
         || (    buf[ 1 ] != '-'
              && buf[ 1 ] != '+'
              && ! isdigit( ( unsigned char ) buf[ 1 ] ) ) )
    {
        print( FATAL, "Error reading setpoint.\n" );
        THROW( EXCEPTION );
    }

    sscanf( buf + 1, "%lf", &setpoint );

    /* If the first character is a '+' or '-' the sensor is returning
       setpoint temperatures in degree Celsius, otherwise in Kelvin */

    if ( ! isdigit( ( unsigned char ) buf[ 1 ] ) )
         setpoint += C2K_OFFSET;

    return setpoint;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
itc503_set_setpoint( double setpoint )
{
    char cmd[ 20 ];
    char buf[ 20 ];


    fsc2_assert( setpoint >= 0.0 && setpoint <= MAX_TEMP );

    sprintf( buf, "T%.3f\r", setpoint );

    if ( itc503_talk( cmd, buf, sizeof buf ) != 2 )
    {
        print( FATAL, "Failed to set setpoint.\n" );
        THROW( EXCEPTION );
    }
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
itc503_set_heater_power_limit( double limit )
{
    char cmd[ 20 ];
    char buf[ 20 ];


    fsc2_assert(    limit >= 0.0
                 && lrnd( 10 * limit ) <= lrnd( 10 * MAX_HEATER_POWER ) );

    sprintf( cmd, "M%.1f\r", limit );
    if ( itc503_talk( cmd, buf, sizeof buf ) != 2 )
    {
        print( FATAL, "Failed to set heater power limit.\n" );
        THROW( EXCEPTION );
    }
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static double
itc503_get_heater_power( void )
{
    char buf[ 20 ];


    if ( itc503_talk( "R5\r", buf, sizeof buf ) < 3 )
    {
        print( FATAL, "Failed to read heater power.\n" );
        THROW( EXCEPTION );
    }

    return T_atod( buf + 1 );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
itc503_set_heater_power( double hp )
{
    char cmd[ 20 ];
    char buf[ 20 ];


    fsc2_assert( hp >= 0 && lrnd( 10 * hp ) <= 999 );

    sprintf( cmd, "O%.1f\r", hp );
    if ( itc503_talk( cmd, buf, sizeof buf ) != 2 )
    {
        print( FATAL, "Failed to set heater power.\n" );
        THROW( EXCEPTION );
    }
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static double
itc503_get_gas_flow( void )
{
    char buf[ 20 ];


    if ( itc503_talk( "R7\r", buf, sizeof buf ) < 3 )
    {
        print( FATAL, "Failed to read gas flow.\n" );
        THROW( EXCEPTION );
    }

    return T_atod( buf + 1 );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
itc503_set_gas_flow( double gf )
{
    char cmd[ 20 ];
    char buf[ 20 ];


    fsc2_assert( gf >= 0 && lrnd( 10 * gf ) <= 999 );

    if ( itc503_get_state( ) > STATE_AUTO )
    {
        print( FATAL, "Can't set gas flow while in AutoGFS phase.\n" );
        THROW( EXCEPTION );
    }

    sprintf( cmd, "G%.1f\r", gf );
    if ( itc503_talk( cmd, buf, sizeof buf ) != 2 )
    {
        print( FATAL, "Failed to set gas flow.\n" );
        THROW( EXCEPTION );
    }
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
itc503_lock( int state )
{
    const char *cmd = NULL;


    switch ( state )
    {
        case LOCAL_AND_LOCKED :
            cmd = "C0\r";
            break;

        case REMOTE_AND_LOCKED :
            cmd = "C1\r";
            break;

        case LOCAL_AND_UNLOCKED :
            cmd = "C2\r";
            break;

        case REMOTE_AND_UNLOCKED :
            cmd = "C3\r";
            break;

        default :
            fsc2_impossible( );
    }

    itc503_command( cmd );
    itc503.lock_state = state;
}


#if 0
/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
itc503_set_auto_pid_state( bool state )
{
    char cmd[ ] = "L*\r";
    char buf[ 10 ];

    cmd[ 1 ] = state ? '1' : '0';

    if ( itc503_talk( cmd, buf, sizeof buf ) != 2 )
    {
        print( FATAL, "Failure to set auto-pid.\n" );
        THROW( EXCEPTION );
    }
}


/*--------------------------------------------*
 *--------------------------------------------*/

static bool
itc503_get_auto_pid_state( void )
{
    char buf[ 50 ];


    if (    itc503_talk( "X\r", buf, sizeof buf ) < 12
         || buf[ 11 ] != 'L'
         || ( buf[ 12 ] != '0' && buf[ 12 ] != '1' ) )
    {
        print( FATAL, "Device returns invalid state data.\n");
        THROW( EXCEPTION );
    }

    return buf[ 12 ] == '1';
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
itc503_set_proportional_band( double prop_bnd )
{

    char cmd[ 30 ];
    char buf[ 10 ];

    sprintf( cnd, "P%.3lf\r", prop_band );

    if ( itc503_talk( cmd, buf, sizeof buf ) != 2 )
    {
        print( FATAL, "Failure to set proportional band.\n" );
        THROW( EXCEPTION );
    }
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double
itc503_get_proportional_band( void )
{
    char buf[ 50 ];
    double prop_band;


    if (    itc503_talk( "R8\r", buf, sizeof buf ) < 2
         || sscanf( buf + 1, "%lf", &prop_band ) != 1 )a
    {
        print( FATAL, "Device returns invalid data.\n");
        THROW( EXCEPTION );
    }

    return prop_band;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
itc503_set_integral_time( double int_time )
{

    char cmd[ 30 ];
    char buf[ 10 ];


    fsc2_assert( int_time >= 0 && int_time < MAX_INTEGRAL_TIME );

    sprintf( cmd, "I%.1d\r", int_time / 6.0 );

    if ( itc503_talk( cmd, buf, sizeof buf ) != 2 )
    {
        print( FATAL, "Failure to set integral time.\n" );
        THROW( EXCEPTION );
    }
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double
itc503_get_integral_time( void )
{
    char buf[ 50 ];
    double int_time;


    if (    itc503_talk( "R9\r", buf, sizeof buf ) < 5
         || sscanf( buf + 1, "%lf", &int_time ) != 1 )
    {
        print( FATAL, "Device returns invalid data.\n");
        THROW( EXCEPTION );
    }

    return 6.0 * int_time;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
itc503_set_derivative_time( double deriv_time )
{

    char cmd[ 30 ];
    char buf[ 10 ];


    fsc2_assert( deriv_time >= 0 && deriv_time < MAX_DERIVATIVE_TIME );

    sprintf( cmd, "D%d\r", irnd( deriv_time / 60 ) );

    if ( itc503_talk( cmd, buf, sizeof buf ) != 2 )
    {
        print( FATAL, "Failure to set derivative time.\n" );
        THROW( EXCEPTION );
    }
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double
itc503_get_derivative_time( void )
{
    char buf[ 50 ];
    double deriv_time;


    if (    itc503_talk( "R10\r", buf, sizeof buf ) < 2
         || sscanf( buf + 1, "%lf", &deriv_time ) != 1 )
    {
        print( FATAL, "Device returns invalid data.\n");
        THROW( EXCEPTION );
    }

    return 60.0 * deriv_time;
}
#endif

/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
itc503_command( const char * cmd )
{
    if ( gpib_write( itc503.device, cmd, strlen( cmd ) ) == FAILURE )
        itc503_gpib_failure( );

    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static long
itc503_talk( const char * message,
             char *       reply,
             long         length )
{
    long len;
    int retries = MAX_RETRIES;


 start:

    if ( gpib_write( itc503.device, message, strlen( message ) ) == FAILURE )
        itc503_gpib_failure( );

 reread:

    stop_on_user_request( );

    len = length - 1;
    if ( gpib_read( itc503.device, reply, &len ) == FAILURE )
        itc503_gpib_failure( );

    /* If device misunderstood the command send it again, repeat up to
       MAX_RETRIES times */

    if ( reply[ 0 ] == '?' )
    {
        if ( retries-- )
            goto start;
        else
            itc503_gpib_failure( );
    }

    /* If the first character of the reply isn't equal to the first character
       of the message we probably read the reply for a previous command and
       try to read again... */

    if ( reply[ 0 ] != message[ 0 ] )
    {
        if ( retries-- )
            goto reread;
        else
            itc503_gpib_failure( );
    }

    reply[ len ] = '\0';

    return len;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
itc503_gpib_failure( void )
{
    print( FATAL, "Communication with device failed.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
