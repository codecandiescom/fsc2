/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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
#include "serial.h"


/* Include configuration information for the device */

#include "hjs_daadc.conf"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define MIN_OUT_VOLTS     0.0
#define MAX_OUT_VOLTS     10.0

#define MAX_IN_VOLTS      10.0

#define HJS_DAADC_ADC_TEST_VOLTAGE 0.0


/* Exported functions */

int hjs_daadc_init_hook(       void );
int hjs_daadc_test_hook(       void );
int hjs_daadc_exp_hook(        void );
int hjs_daadc_end_of_exp_hook( void );
void hjs_daadc_exit_hook(      void );

Var_T *daq_name(                   Var_T *v );
Var_T *daq_reserve_dac(            Var_T *v );
Var_T *daq_reserve_adc(            Var_T *v );
Var_T *daq_maximum_output_voltage( Var_T *v );
Var_T *daq_set_voltage(            Var_T *v );
Var_T *daq_get_voltage(            Var_T *v );
Var_T *daq_dac_parameter(          Var_T *v );


static int hjs_daadc_da_volts_to_val( double volts );

static double hjs_daadc_val_to_ad_volts( int val );

static bool hjs_daadc_serial_init( void );

static void hjs_daadc_out( int out );

static int hjs_daadc_in( void );

static int hjs_daadc_in_out( int out );

static void hjs_daadc_comm_failure( void );


struct HJS_DAADC {
    bool is_open;
    struct termios *tio;    /* serial port terminal interface structure */
    double max_volts;
    double volts_out;
    bool is_volts_out;
    int out_val;

    char *dac_reserved_by;
    char *adc_reserved_by;
};

struct HJS_DAADC hjs_daadc, hjs_daadc_stored;


/*-------------------------------------------*/
/*                                           */
/*            Hook functions                 */
/*                                           */
/*-------------------------------------------*/

/*------------------------------------------------------*
 * Function called when the module has just been loaded
 *------------------------------------------------------*/

int hjs_daadc_init_hook( void )
{
    fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

    hjs_daadc.is_open   = UNSET;
    hjs_daadc.out_val   = DEF_OUT_VAL;
    hjs_daadc.max_volts = MAX_OUT_VOLTS;
    hjs_daadc.volts_out = MAX_OUT_VOLTS * DEF_OUT_VAL / 4095.0;

    hjs_daadc.dac_reserved_by = NULL;
    hjs_daadc.adc_reserved_by = NULL;

    return 1;
}


/*----------------------------------------------*
 * Function called at the start of the test run
 *----------------------------------------------*/

int hjs_daadc_test_hook( void )
{
    hjs_daadc_stored = hjs_daadc;

    if ( hjs_daadc.dac_reserved_by )
        hjs_daadc_stored.dac_reserved_by =
                                         T_strdup( hjs_daadc.dac_reserved_by );

    if ( hjs_daadc.adc_reserved_by )
        hjs_daadc_stored.adc_reserved_by =
                                         T_strdup( hjs_daadc.adc_reserved_by );

    return 1;
}


/*-----------------------------------------------*
 * Function called at the start of an experiment
 *-----------------------------------------------*/

int hjs_daadc_exp_hook( void )
{
    /* Restore state from before the start of the test run (or the
       last experiment) */

    if ( hjs_daadc.dac_reserved_by )
        hjs_daadc.dac_reserved_by = CHAR_P T_free( hjs_daadc.dac_reserved_by );

    if ( hjs_daadc.adc_reserved_by )
        hjs_daadc.adc_reserved_by = CHAR_P T_free( hjs_daadc.adc_reserved_by );

    hjs_daadc = hjs_daadc_stored;

    if ( hjs_daadc_stored.dac_reserved_by )
        hjs_daadc.dac_reserved_by =
                                  T_strdup( hjs_daadc_stored.dac_reserved_by );

    if ( hjs_daadc_stored.adc_reserved_by )
        hjs_daadc.adc_reserved_by =
                                  T_strdup( hjs_daadc_stored.adc_reserved_by );


    if ( ! ( hjs_daadc.is_open = hjs_daadc_serial_init( ) ) )
        hjs_daadc_comm_failure( );

    hjs_daadc_out( DEF_OUT_VAL );
    
    return 1;
}


/*---------------------------------------------*
 * Function called at the end of an experiment
 *---------------------------------------------*/

int hjs_daadc_end_of_exp_hook( void )
{
    if ( hjs_daadc.is_open )
        fsc2_serial_close( SERIAL_PORT );
    hjs_daadc.is_open = UNSET;
    return 1;
}


/*------------------------------------------------------*
 * Function called just before the module gets unloaded
 *------------------------------------------------------*/

void hjs_daadc_exit_hook( void )
{
    if ( hjs_daadc.dac_reserved_by )
        T_free( hjs_daadc.dac_reserved_by );
    if ( hjs_daadc_stored.dac_reserved_by )
        T_free( hjs_daadc_stored.dac_reserved_by );

    if ( hjs_daadc.adc_reserved_by )
        T_free( hjs_daadc.adc_reserved_by );
    if ( hjs_daadc_stored.adc_reserved_by )
        T_free( hjs_daadc_stored.adc_reserved_by );
}


/*-------------------------------------------*/
/*                                           */
/*             EDL functions                 */
/*                                           */
/*-------------------------------------------*/

/*----------------------------------------------------------------*
 * Function returns a string variable with the name of the device
 *----------------------------------------------------------------*/

Var_T *daq_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*--------------------------------------------------------------------*
 * Functions allows to reserve (or un-reserve) the DAC so that in the
 * following setting the DAC requires a pass-phrase as the very first
 * argument to the functions daq_maximum_output_voltage() and
 * daq_set_voltage().
 *--------------------------------------------------------------------*/

Var_T *daq_reserve_dac( Var_T * v )
{
    bool lock_state = SET;


    if ( v == NULL )
        return vars_push( INT_VAR, hjs_daadc.dac_reserved_by ? 1L : 0L );

    /* Get the pass-phrase */

    if ( v->type != STR_VAR )
    {
        print( FATAL, "First argument isn't a string.\n" );
        THROW( EXCEPTION );
    }

    /* The next, optional argument tells if the DAC is to be locked or
       unlocked (if it's missing it defaults to locking the DAC) */

    if ( v->next != NULL )
    {
        lock_state = get_boolean( v->next );
        too_many_arguments( v->next );
    }

    /* Lock or unlock (after checking the pass-phrase) the DAC */

    if ( hjs_daadc.dac_reserved_by )
    {
        if ( lock_state == SET )
        {
            if ( ! strcmp( hjs_daadc.dac_reserved_by, v->val.sptr ) )
                return vars_push( INT_VAR, 1L );
            else
                return vars_push( INT_VAR, 0L );
        }
        else
        {
            if ( ! strcmp( hjs_daadc.dac_reserved_by, v->val.sptr ) )
            {
                hjs_daadc.dac_reserved_by =
                                    CHAR_P T_free( hjs_daadc.dac_reserved_by );
                return vars_push( INT_VAR, 1L );
            }
            else
                return vars_push( INT_VAR, 0L );
        }
    }

    if ( ! lock_state )
        return vars_push( INT_VAR, 1L );

    hjs_daadc.dac_reserved_by = T_strdup( v->val.sptr );
    return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------------------------*
 * Function allows to reserve (or un-reserve) the ADC so that in the
 * following setting the DAC requires a pass-phrase as the very first
 * argument to the function daq_get_voltage().
 *--------------------------------------------------------------------*/

Var_T *daq_reserve_adc( Var_T * v )
{
    bool lock_state = SET;


    if ( v == NULL )
        return vars_push( INT_VAR, hjs_daadc.adc_reserved_by ? 1L : 0L );

    /* Get the pass-phrase */

    if ( v->type != STR_VAR )
    {
        print( FATAL, "First argument isn't a string.\n" );
        THROW( EXCEPTION );
    }

    /* The next, optional argument tells if the ADC is to be locked or
       unlocked (if it's missing it defaults to locking the ADC) */

    if ( v->next != NULL )
    {
        lock_state = get_boolean( v->next );
        too_many_arguments( v->next );
    }

    /* Lock or unlock (after checking the pass-phrase) the ADC */

    if ( hjs_daadc.adc_reserved_by )
    {
        if ( lock_state == SET )
        {
            if ( ! strcmp( hjs_daadc.adc_reserved_by, v->val.sptr ) )
                return vars_push( INT_VAR, 1L );
            else
                return vars_push( INT_VAR, 0L );
        }
        else
        {
            if ( ! strcmp( hjs_daadc.adc_reserved_by, v->val.sptr ) )
            {
                hjs_daadc.adc_reserved_by =
                                    CHAR_P T_free( hjs_daadc.adc_reserved_by );
                return vars_push( INT_VAR, 1L );
            }
            else
                return vars_push( INT_VAR, 0L );
        }
    }

    if ( ! lock_state )
        return vars_push( INT_VAR, 1L );

    hjs_daadc.adc_reserved_by = T_strdup( v->val.sptr );
    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------------------------*
 * Function can be used to inform the module about the current settinng
 * of the potentiometer at the front panel. It takes a floating point
 * input argument between 0 and 10 (that should be identical to the
 * scale of the potentiometer). In later calls of daq_set_voltage() it
 * will be then assumed that this is the correct potentiometer setting
 * and the output voltage is adjusted accordingly. If, for example, the
 * function is called with an argument of 2.5 and later the function
 * daq_set_voltage() is called with an argument of 1.25 V the value
 * send to the DAC is halve of the maximum value instead of an eigth of
 * it, thus the real output voltage is 1.25 V (at least if the value
 * passed to the function about the potentiometer setting was correct.
 *----------------------------------------------------------------------*/

Var_T *daq_maximum_output_voltage( Var_T * v )
{
    double volts;
    char *pass = NULL;


    /* If the first argument is a string we assume it's a pass-phrase */

    if ( v != NULL && v->type == STR_VAR )
    {
        pass = v->val.sptr;
        v = v->next;
    }

    /* If there's no other argument return the current settting of the
       maximum output voltage */

    if ( v == NULL )
        return vars_push( FLOAT_VAR, hjs_daadc.max_volts );

    /* Get the new maximum output voltage */

    volts = get_double( v, "DAC output voltage" );

    /* Allow changes only when the DAC is either not locked or the
       pass-phrase is correct */

    if ( hjs_daadc.dac_reserved_by )
    {
        if ( pass == NULL )
        {
            print( FATAL, "DAC is reserved, phase-phrase required.\n" );
            THROW( EXCEPTION );
        }
        else if ( strcmp( hjs_daadc.dac_reserved_by, pass ) )
        {
            print( FATAL, "DAC is reserved, wrong phase-phrase.\n" );
            THROW( EXCEPTION );
        }
    }       

    too_many_arguments( v );

    /* Check the new maximum output voltage */

    if ( volts < MIN_OUT_VOLTS || volts > MAX_OUT_VOLTS )
    {
        if ( FSC2_MODE == EXPERIMENT )
        {
            print( SEVERE, "Maximum output voltage of %f V too %s, must be "
                   "between %.2f V and %.2f V. Keeping current maximum output "
                   "voltage of %.2f V.\n", volts, volts < MIN_OUT_VOLTS ?
                   "low" : "high", MIN_OUT_VOLTS, MAX_OUT_VOLTS,
                   hjs_daadc.max_volts );
            return vars_push( FLOAT_VAR, hjs_daadc.max_volts );
        }
        else
        {
            print( FATAL, "Maximim output voltage of %f V too %s, must be "
                   "between %.2f V and %.2f V.\n", volts,
                   volts < MIN_OUT_VOLTS ? "low" : "high", MIN_OUT_VOLTS,
                   MAX_OUT_VOLTS );
            THROW( EXCEPTION );
        }
    }

    hjs_daadc.max_volts = volts;

    return vars_push( FLOAT_VAR, hjs_daadc.max_volts );
}


/*----------------------------------------------------------------------*
 * Function for either querying the current output voltage or setting a
 * new output voltage. The output voltage must be between 0 V and the
 * maximum voltage set by the function daq_maximum_output_voltage() (if
 * the function hasn't been called the maximum output voltage defaults
 * to the highest possible value of 10 V).
 *----------------------------------------------------------------------*/

Var_T *daq_set_voltage( Var_T * v )
{
    double volts;
    char *pass = NULL;


    /* If the first argument is a string we assume it's a pass-phrase */

    if ( v != NULL && v->type == STR_VAR )
    {
        pass = v->val.sptr;
        v = v->next;
    }

    /* If there's no other argument return the currently set output voltage
       (if it's possible) */

    if ( v == NULL )
        return vars_push( FLOAT_VAR, hjs_daadc.volts_out );

    /* Get the new output voltage */

    volts = get_double( v, "DAC output voltage" );

    /* Allow changes only if either the DAC isn't locked or the pass-phrase
       is correct */

    if ( hjs_daadc.dac_reserved_by )
    {
        if ( pass == NULL )
        {
            print( FATAL, "DAC is reserved, phase-phrase required.\n" );
            THROW( EXCEPTION );
        }
        else if ( strcmp( hjs_daadc.dac_reserved_by, pass ) )
        {
            print( FATAL, "DAC is reserved, wrong phase-phrase.\n" );
            THROW( EXCEPTION );
        }
    }       

    too_many_arguments( v );

    /* Check the new output voltage */

    if ( volts < MIN_OUT_VOLTS || volts >= hjs_daadc.max_volts * 1.0001 )
    {
        if ( FSC2_MODE == EXPERIMENT )
        {
            print( SEVERE, "Output voltage of %f V too %s, must be between "
                   "%.2f V and %.2f V. Keeping current voltage of %.2f V.\n",
                   volts, volts < MIN_OUT_VOLTS ? "low" : "high",
                   MIN_OUT_VOLTS, hjs_daadc.max_volts, hjs_daadc.volts_out );
            return vars_push( FLOAT_VAR, hjs_daadc.volts_out );
        }
        else
        {
            print( FATAL, "Output voltage of %f V too %s, must be between "
                   "%.2f V and %.2f V.\n", volts, volts < MIN_OUT_VOLTS ?
                   "low" : "high", MIN_OUT_VOLTS, hjs_daadc.max_volts );
            THROW( EXCEPTION );
        }
    }

    /* Set the new output voltage */

    if ( FSC2_MODE == EXPERIMENT )
        hjs_daadc_out( hjs_daadc_da_volts_to_val( volts ) );

    hjs_daadc.volts_out = volts;

    return vars_push( FLOAT_VAR, hjs_daadc.volts_out );
}


/*---------------------------------------------------------------------*
 * Function for determining the input voltage at the ADC input. It can
 * be between -10 V and +10 V. Please note that, because a conversion
 * is only done when a new output voltage is set, getting a new input
 * voltage may require setting the output voltage of the DAC to a
 * default value (this happens when daq_get_voltage() has never been
 * called yet).
 *---------------------------------------------------------------------*/

Var_T *daq_get_voltage( Var_T * v )
{
    char *pass = NULL;


    /* If the first argument is a string we assume it's a pass-phrase */

    if ( v != NULL )
    {
        if ( v->type == STR_VAR )
            pass = v->val.sptr;
        else
        {
            print( FATAL, "Invalid argument.\n" );
            THROW( EXCEPTION );
        }
    }

    /* Allow reading the ADC only if either the ADC isn't locked or if
       the pas-phrase is correct */

    if ( hjs_daadc.adc_reserved_by )
    {
        if ( pass == NULL )
        {
            print( FATAL, "ADC is reserved, phase-phrase required.\n" );
            THROW( EXCEPTION );
        }
        else if ( strcmp( hjs_daadc.adc_reserved_by, pass ) )
        {
            print( FATAL, "ADC is reserved, wrong phase-phrase.\n" );
            THROW( EXCEPTION );
        }
    }       

    /* The AD conversion only happens when there's a simultaneous DA
       conversion. If none has been done before reading the ADC involves
       setting the DAC to a default output voltage and we better tell
       the user about it */

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( FLOAT_VAR, HJS_DAADC_ADC_TEST_VOLTAGE );

    return vars_push( FLOAT_VAR,
                      hjs_daadc_val_to_ad_volts( hjs_daadc_in( ) ) );
}


/*------------------------------------------------------*
 * Function for returning the DAC settings (minimum and
 * maximum output voltage and voltage resolution).
 *------------------------------------------------------*/

Var_T *daq_dac_parameter( Var_T * v )
{
    double params[ 3 ];


    /* If the first argument is a string we assume it's a pass-phrase
       (which we acually don't require). */

    if ( v != NULL && v->type == STR_VAR )
        v = vars_pop( v );

    if ( v != NULL && v->next == NULL )
    {
        print( WARN, "Function does not take an argument for this DAQ.\n" );
        v = vars_pop( v );
    }

    too_many_arguments( v );

    params[ 0 ] = MIN_OUT_VOLTS;
    params[ 1 ] = hjs_daadc.max_volts;
    params[ 2 ] = ( params[ 1 ] - params[ 0 ] ) / 4095.0;

    return vars_push( FLOAT_ARR, params, ( ssize_t ) 3 );
}


/*-------------------------------------------*/
/*                                           */
/*           Low level functions             */
/*                                           */
/*-------------------------------------------*/

/*-----------------------------------------------*
 * Conversion of a voltage to a DAC output value
 *-----------------------------------------------*/

static int hjs_daadc_da_volts_to_val( double volts )
{
    fsc2_assert( volts >= MIN_OUT_VOLTS &&
                 volts < hjs_daadc.max_volts * 1.0001 );

    return irnd( volts / hjs_daadc.max_volts * 4095.0 );
}


/*-----------------------------------------------*
 * Conversion of an ADC input value to a voltage
 *-----------------------------------------------*/

static double hjs_daadc_val_to_ad_volts( int val )
{
    return MAX_IN_VOLTS * val / 2047.0;
}


/*---------------------------------------------------------------*
 * Initialization of the serial port the device is connected to.
 *---------------------------------------------------------------*/

static bool hjs_daadc_serial_init( void )
{
    if ( ( hjs_daadc.tio = fsc2_serial_open( SERIAL_PORT, DEVICE_NAME,
                                 O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) )
         == NULL )
        return FAIL;

    /* The device uses 8 bit transfers, no parity and one stop bit (8N1),
       a baud rate of 9600 and has no control lines (only RTX, DTX and
       ground are connected internally). Of course, canonical mode can't
       be used here.*/

    memset( hjs_daadc.tio, 0, sizeof *hjs_daadc.tio );
    hjs_daadc.tio->c_cflag = CS8 | CSIZE | CREAD | CLOCAL;

    cfsetispeed( hjs_daadc.tio, SERIAL_BAUDRATE );
    cfsetospeed( hjs_daadc.tio, SERIAL_BAUDRATE );

    fsc2_tcflush( SERIAL_PORT, TCIOFLUSH );
    fsc2_tcsetattr( SERIAL_PORT, TCSANOW, hjs_daadc.tio );

    return OK;
}


/*---------------------------------------------------*
 * Function for outputting a value at the DAC output
 *---------------------------------------------------*/

static void hjs_daadc_out( int out )
{
    fsc2_assert( out >= 0 && out <= 4095 );
    hjs_daadc_in_out( out );
    hjs_daadc.out_val = out;
}


/*-----------------------------------------------*
 * Function for getting a new value from the ADC
 *-----------------------------------------------*/

static int hjs_daadc_in( void )
{
    return hjs_daadc_in_out( hjs_daadc.out_val );
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

static int hjs_daadc_in_out( int out )
{
    unsigned char out_bytes[ 4 ] = { 0x00, 0x60, 0xd0, 0x70 };
    unsigned char in_bytes[ 4 ];
    unsigned int in_val;


    /* Split up the 12-bit input data into 4-bit chunks and store them in
       lower nibbles of the output bytes. The upper nibbles are control
       bits. The fourth byte to be send is control data only, telling the
       DAC and output the data (and at the same time to convert the
       voltage at the input of the ADC). */

    out_bytes[ 0 ] |= out & 0x0F;
    out_bytes[ 1 ] |= ( out >>= 4 ) & 0x0F;
    out_bytes[ 2 ] |= out >> 4;

    /* We can always write, even if the device is switched off, because
       there exist no control lines, checking the return value is just
       cosmetics ;-). The data from the ADC should have arrived within
       20 ms. */

    if ( fsc2_serial_write( SERIAL_PORT, out_bytes, 4, 0, UNSET ) != 4 ||
         fsc2_serial_read( SERIAL_PORT, in_bytes, 4, 20000, UNSET ) != 4 )
        hjs_daadc_comm_failure( );

    /* The results of the conversion by the ADC is stored in the second and
       third byte of the read in data in the following way: the lower 7 bits
       of the second byte make up the bits 11 to 4 of the result and the upper
       nibble of the third byte the bits 3 to 0. From this value we have to
       subtract 2048 because a value of 0 corresponds to an input voltage
       of -10 V and a value of 4095 to a voltage of +10 V. */

    in_val = ( in_bytes[ 1 ] << 4 ) + ( in_bytes[ 2 ] >> 4 );

    if ( in_val > 4095 )
    {
        print( FATAL, "ADC returned flawed data.\n" );
        THROW( EXCEPTION );
    }

    return in_val - 2048;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

static void hjs_daadc_comm_failure( void )
{
    print( FATAL, "Can't access the AD/DA converter.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
