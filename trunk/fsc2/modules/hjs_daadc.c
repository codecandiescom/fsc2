/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "hjs_daadc.conf"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define MIN_OUT_VOLTS     0.0
#define MAX_OUT_VOLTS     10.0

#define MAX_IN_VOLTS      10.0

#define HJS_DAADC_ADC_TEST_VOLTAGE 0.0


/* Exported functions */

int hjs_daadc_init_hook( void );
int hjs_daadc_test_hook( void );
int hjs_daadc_exp_hook( void );
int hjs_daadc_end_of_exp_hook( void );
void hjs_daadc_exit_hook( void );

Var *daq_name( Var *v );
Var *daq_maximum_output_voltage( Var *v );
Var *daq_set_voltage( Var *v );
Var *daq_get_voltage( Var *v );

static double hjs_daadc_val_to_da_volts( int val );
static int hjs_daadc_da_volts_to_val( double volts );
static double hjs_daadc_val_to_ad_volts( int val );
static bool hjs_daadc_serial_init( void );
static void hjs_daadc_out( int out );
static int hjs_daadc_in( void );
static int hjs_daadc_in_out( int out );
static void hjs_daadc_comm_failure( void );


typedef struct HJS_DAADC HJS_DAADC;

struct HJS_DAADC {
	bool is_open;
    struct termios *tio;    /* serial port terminal interface structure */
	double max_volts;
	double volts_out;
	bool is_volts_out;
	int out_val;
	bool has_dac_been_set;;
};


HJS_DAADC hjs_daadc, hjs_daadc_stored;


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int hjs_daadc_init_hook( void )
{
	fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

	hjs_daadc.is_open   = UNSET;
	hjs_daadc.out_val   = DEF_OUT_VAL;
	hjs_daadc.max_volts = MAX_OUT_VOLTS;
	hjs_daadc.volts_out = hjs_daadc_val_to_da_volts( hjs_daadc.out_val );
	hjs_daadc.has_dac_been_set = UNSET;

	return 1;
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int hjs_daadc_test_hook( void )
{
	hjs_daadc_stored = hjs_daadc;
	return 1;
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int hjs_daadc_exp_hook( void )
{
	/* Restore state from before the start of the test run */

	hjs_daadc = hjs_daadc_stored;

	if ( ! ( hjs_daadc.is_open = hjs_daadc_serial_init( ) ) )
		hjs_daadc_comm_failure( );
	
	return 1;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int hjs_daadc_end_of_exp_hook( void )
{
	if ( hjs_daadc.is_open )
		fsc2_serial_close( SERIAL_PORT );
	hjs_daadc.is_open = UNSET;
	return 1;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void hjs_daadc_exit_hook( void )
{
	/* This shouldn't be necessary, but to make 100% sure the device file
	   is closed wedo it anyway */

	if ( hjs_daadc.is_open )
		fsc2_serial_close( SERIAL_PORT );
	hjs_daadc.is_open = UNSET;
}


/*----------------------------------------------------------------*/
/* Function returns a string variable with the name of the device */
/*----------------------------------------------------------------*/

Var *daq_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------------------------*/
/* Function can be used to inform the module about the current settinng */
/* of the potentiometer at the front panel. It takes a floating point   */
/* input argument between 0 and 10 (that should be identical to the     */
/* scale of the potentiometer). In later calls of daq_set_voltage() it  */
/* will be then assumed that this is the correct potentiometer setting  */
/* and the output voltage is adjusted accordingly. If, for example, the */
/* function is called with an argument of 2.5 and later the function    */
/* daq_set_voltage() is called with an argument of 1.25 V the value     */
/* send to the DAC is halve of the maximum value instead of an eigth of */
/* it, thus the real output voltage is 1.25 V (at least if the value    */
/* passed to the function about the potentiometer setting was correct.  */
/*----------------------------------------------------------------------*/

Var *daq_maximum_output_voltage( Var *v )
{
	double volts;


	if ( v == NULL )
		return vars_push( FLOAT_VAR, hjs_daadc.max_volts );

	volts = get_double( v, "DAC output voltage" );

	too_many_arguments( v );

	if ( volts < MIN_OUT_VOLTS || volts > MAX_OUT_VOLTS )
	{
		if ( FSC2_MODE == EXPERIMENT )
		{
			print( SEVERE, "Maximum output voltage of %f too %s, must be "
				   "between %.2f V and %.2f V. Keeping current maximum output "
				   "voltage of %.2f V.\n", volts, volts < MIN_OUT_VOLTS ?
				   "low" : "high", MIN_OUT_VOLTS, MAX_OUT_VOLTS,
				   hjs_daadc.max_volts );
			return vars_push( FLOAT_VAR, hjs_daadc.max_volts );
		}
		else
		{
			print( FATAL, "Maximim output voltage of %f too %s, must be "
				   "between %.2f V and %.2f V.\n", volts,
				   volts < MIN_OUT_VOLTS ? "low" : "high", MIN_OUT_VOLTS,
				   MAX_OUT_VOLTS );
			THROW( EXCEPTION );
		}
	}

	hjs_daadc.max_volts = volts;

	return vars_push( FLOAT_VAR, hjs_daadc.max_volts );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *daq_set_voltage( Var *v )
{
	double volts;


	if ( v == NULL )
	{
		if ( ! hjs_daadc.has_dac_been_set )
		{
			print( FATAL, "DAC hasn't been set before, can't determine the "
				   "current output voltage.\n" );
			THROW( EXCEPTION );
		}

		return vars_push( FLOAT_VAR, hjs_daadc.volts_out );
	}

	volts = get_double( v, "DAC output voltage" );

	too_many_arguments( v );

	if ( volts < MIN_OUT_VOLTS || volts >= hjs_daadc.max_volts * 1.001 )
	{
		if ( FSC2_MODE == EXPERIMENT )
		{
			print( SEVERE, "Output voltage of %f too %s, must be between "
				   "%.2f V and %.2f V. Keeping current voltage of %.2f V.\n",
				   volts, volts < MIN_OUT_VOLTS ? "low" : "high",
				   MIN_OUT_VOLTS, hjs_daadc.max_volts, hjs_daadc.volts_out );
			return vars_push( FLOAT_VAR, hjs_daadc.volts_out );
		}
		else
		{
			print( FATAL, "Output voltage of %f too %s, must be between "
				   "%.2f V and %.2f V.\n", volts, volts < MIN_OUT_VOLTS ?
				   "low" : "high", MIN_OUT_VOLTS, hjs_daadc.max_volts );
			THROW( EXCEPTION );
		}
	}

	if ( FSC2_MODE == EXPERIMENT )
		hjs_daadc_out( hjs_daadc_da_volts_to_val( volts ) );

	hjs_daadc.volts_out = volts;
	hjs_daadc.has_dac_been_set = SET;

	return vars_push( FLOAT_VAR, hjs_daadc.volts_out );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *daq_get_voltage( Var *v )
{
	UNUSED_ARGUMENT ( v );


	if ( ! hjs_daadc.has_dac_been_set )
		print( SEVERE, "AD conversion requires DA conversion, setting "
			   "DAC output voltage to %.2lf V.\n", hjs_daadc.volts_out );

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( FLOAT_VAR, HJS_DAADC_ADC_TEST_VOLTAGE );

	return vars_push( FLOAT_VAR,
					  hjs_daadc_val_to_ad_volts( hjs_daadc_in( ) ) );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static double hjs_daadc_val_to_da_volts( int val )
{
	return hjs_daadc.max_volts * val / 4095.0;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static int hjs_daadc_da_volts_to_val( double volts )
{
	fsc2_assert( volts >= MIN_OUT_VOLTS &&
				 volts < hjs_daadc.max_volts * 1.001 );

	return irnd( volts / hjs_daadc.max_volts * 4095.0 );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static double hjs_daadc_val_to_ad_volts( int val )
{
	return MAX_IN_VOLTS * val / 2047.0;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static bool hjs_daadc_serial_init( void )
{
	if ( ( hjs_daadc.tio = fsc2_serial_open( SERIAL_PORT, DEVICE_NAME,
								 O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) )
		 == NULL )
		return FAIL;

	/* The device uses 8 bit transfers, no parity and one stop bit (8N1),
	   a baud rate of 9600 and had no control lines (only RTX, DTX and
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


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static void hjs_daadc_out( int out )
{
    fsc2_assert( out >= 0 && out <= 4095 );
	hjs_daadc_in_out( out );
	hjs_daadc.out_val = out;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static int hjs_daadc_in( void )
{
	return hjs_daadc_in_out( hjs_daadc.out_val );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static int hjs_daadc_in_out( int out )
{
	unsigned char out_bytes[ 4 ] = { 0x00, 0x60, 0xd0, 0x70 };
	unsigned char in_bytes[ 4 ];


	/* Split up the 12 bit input data into 4-bit chunks and store them in
	   lower nibbles of the output bytes. The upper nibbles are control
	   bits. The fourth byte to be send is control data only, telling the
	   DAC to output the data. */

	out_bytes[ 0 ] |= out & 0x0F;
	out_bytes[ 1 ] |= ( out >>= 4 ) & 0x0F;
	out_bytes[ 2 ] |= out >> 4;

	/* We can always write, even if the device is switched off, because
	   there exist no control lines, checking the return value is just
	   cosmetics ;-). The data from the conversion should be available
	   within 20 ms. */

	if ( fsc2_serial_write( SERIAL_PORT, out_bytes, 4 ) != 4 ||
		 fsc2_serial_read( SERIAL_PORT, in_bytes, 4, 20000, UNSET ) != 4 )
		hjs_daadc_comm_failure( );

	/* The results of the conversion by the ADC is stored in the second and
	   third byte of the read in data in the following way: the lower 7 bits
	   of the second byte make up the bits 4 to 11 of the result and the upper
	   nibble of the third byte the bits 0 to 3. From this value we have to
	   subtract 2048 because a value of 0 corresponds to an input voltage
	   of -10 V and a value of 4095 to a voltage of +10 V. */

	return ( in_bytes[ 1 ] << 4 ) + ( in_bytes[ 2 ] >> 4 ) - 2048;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void hjs_daadc_comm_failure( void )
{
	print( FATAL, "Can't access the AD/DA converter.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
