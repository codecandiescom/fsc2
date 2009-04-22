/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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
#include "gpib_if.h"


/* Include configuration information for the device */

#include "eip371.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


int eip371_init_hook(       void );
int eip371_exp_hook(        void );
int eip371_end_of_exp_hook( void );


Var_T * freq_counter_name(       Var_T * v );
Var_T * freq_counter_band(       Var_T * v );
Var_T * freq_counter_measure(    Var_T * v );
Var_T * freq_counter_resolution( Var_T * v );
Var_T * freq_counter_command(    Var_T * v );


static bool eip371_init( const char * name );
static double eip371_set_band( int band );
static double eip371_set_resolution( int res );
static double eip371_get_freq( void );
static bool eip371_command( const char * cmd );
static void eip371_comm_fail( void );


static struct {
    int device;
	int band;
	int res;
	bool is_res;
} eip371;


/* Define the frequency that the module returns during the test run */

#define EIP371_TEST_FREQUENCY  9.2e-9


/**********************************************************/
/*                                                        */
/*                  hook functions                        */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
eip371_init_hook( void )
{
    /* Set global variable to indicate that GPIB bus is needed */

    Need_GPIB = SET;

    eip371.device = -1;
	eip371.band = 3;
	eip371.is_res = UNSET;

    return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
eip371_exp_hook( void )
{
    if ( ! eip371_init( device_name ) )
    {
        print( FATAL, "Initialization of device failed: %s\n",
               gpib_error_msg );
        THROW( EXCEPTION );
    }

    return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
eip371_end_of_exp_hook( void )
{
    /* Switch device to local mode */

    if ( eip371.device != -1 )
        gpib_local( eip371.device );

    eip371.device = -1;
    return 1;
}


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
freq_counter_band( Var_T * v )
{
	long band;


	if ( v == NULL )
        return vars_push( INT_VAR, ( long ) eip371.band );

	band = get_long( v, "frequency band" );

	if ( band < 0 || band > 3 )
	{
		print( FATAL, "Invalid band number, must be between 0 and 3.\n" );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	eip371.band = band;

	if ( FSC2_MODE == EXPERIMENT )
		eip371_set_band( band );

	return vars_push( INT_VAR, band );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
freq_counter_resolution( Var_T * v )
{
	size_t i;
	double res[ ] = { 1.0e0, 1.0e1, 1.0e2, 1.0e3, 1.0e4, 1.0e5, 1.0e6 };
	double resolution;


	if ( v == NULL )
    {
        if ( ! eip371.is_res )
        {
            print( FATAL, "Frequency resolution can only be queried after "
                   "having been set.\n" );
            THROW( EXCEPTION );
        }
        else
            return vars_push( FLOAT_VAR, res[ eip371.res ] );
    }

	resolution = get_double( v, "frequency resolution" );

	too_many_arguments( v );

	if ( resolution < 0.99 )
	{
		print( SEVERE, "Resolution too high, switching to 1 Hz\n" );
		i = 0;
	}
	else
	{
        for ( i = 0; i < 6; i++ )
            if ( resolution >= res[ i ] && resolution <= res[ i + 1 ] )
            {
                if ( res[ i ] / resolution < resolution / res[ i + 1 ] )
                    i++;
                break;
            }

		if ( resolution > 1.01e6 )
		{
			print( SEVERE, "Resolution too low, switching to 1 MHz\n" );
			resolution = res[ 6 ];
			i = 6;
		}

		if ( fabs( resolution - res[ i ] ) > 0.01 * resolution )
			print( SEVERE, "Requested resolution can't be set exactly, "
				   "using %.3f Hz  instead.\n", res[ i ] );
	}

	eip371.res = i;
	eip371.is_res = SET;

	if ( FSC2_MODE == EXPERIMENT )
		eip371_set_resolution( i );

	return vars_push( FLOAT_VAR, res[ i ] );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
freq_counter_measure( Var_T * v )
{
    too_many_arguments( v );

    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, EIP371_TEST_FREQUENCY );

    return vars_push( FLOAT_VAR, eip371_get_freq( ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
freq_counter_command( Var_T * v )
{
    char *cmd = NULL;


    CLOBBER_PROTECT( cmd );

    vars_check( v, STR_VAR );

    if ( FSC2_MODE == EXPERIMENT )
    {
        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            eip371_command( cmd );
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
 *--------------------------------------------------------*/

static bool
eip371_init( const char * name )
{
    if ( gpib_init_device( name, &eip371.device ) == FAILURE )
        return FAIL;

    /* Tell device to output data in scientific format and to go into fast
       cycle mode. */

    if ( gpib_write( eip371.device, "ESFA", 4 ) == FAILURE )
        return FAIL;

    /* Set a band (unless specified in the PREPARATIONS section use band 3
       per default) since without a band being set the device doesn't do
       measurements */

    eip371_set_band( eip371.band );

	if ( eip371.is_res )
		eip371_set_resolution( eip371.res );

    return OK;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static double
eip371_set_band( int band )
{
	char cmd[ 2 ] = "B";


	fsc2_assert( band >= 0 && band <= 3 );

	cmd[ 1 ] = '0' + band;
    if ( gpib_write( eip371.device, cmd, 2 ) == FAILURE )
		eip371_comm_fail( );
    return OK;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static double
eip371_set_resolution( int res )
{
	char cmd[ 2 ] = "R";


	fsc2_assert( res >= 0 && res <= 6 );

	cmd[ 1 ] = '0' + res;
    if ( gpib_write( eip371.device, cmd, 2 ) == FAILURE )
		eip371_comm_fail( );
    return OK;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static double
eip371_get_freq( void )
{
    char buf[ 16 ];
    long len = sizeof buf;
	size_t i = 0;


    if ( gpib_read( eip371.device, buf, &len ) == FAILURE || len != 16 )
    {
        print( FATAL, "Communication with device failed.\n" );
        THROW( EXCEPTION );
    }

	buf[ 14 ] = '\0';

	for ( i = 0; buf[ i ] == '\0' && i < 14; i++ )
		/* empty */ ;

    if ( buf[ i ] == '\0' )
    {
        print( FATAL, "Device send invalid data.\n" );
        THROW( EXCEPTION );
    }

    return T_atod( buf + i );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
eip371_command( const char * cmd )
{
    if ( gpib_write( eip371.device, cmd, strlen( cmd ) ) == FAILURE )
    {
        print( FATAL, "Communication with device failed.\n" );
        THROW( EXCEPTION );
    }

    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
eip371_comm_fail( void )
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
