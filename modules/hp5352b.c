/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
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

#include "hp5352b.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


int hp5352b_init_hook(       void );
int hp5352b_exp_hook(        void );
int hp5352b_end_of_exp_hook( void );


Var_T * freq_counter_name(       Var_T * v );
Var_T * freq_counter_measure(    Var_T * v );
Var_T * freq_counter_resolution( Var_T * v );
Var_T * freq_counter_command(    Var_T * v );


static bool hp5352b_init( const char * name );

static double hp5352b_get_freq( void );

static double hp5352b_set_resolution( int );
static bool hp5352b_command( const char * );


static struct {
    int device;
	int res;
	bool is_res;
} hp5352b;


/* Define the frequency that the driver returns during the test run */

#define HP5352B_TEST_FREQUENCY  9.3e9



/**********************************************************/
/*                                                        */
/*                  hook functions                        */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
hp5352b_init_hook( void )
{
    /* Set global variable to indicate that GPIB bus is needed */

    Need_GPIB = SET;

    hp5352b.device = -1;
	hp5352b.is_res = UNSET;
    return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
hp5352b_exp_hook( void )
{
    if ( ! hp5352b_init( device_name ) )
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
hp5352b_end_of_exp_hook( void )
{
    if ( hp5352b.device != -1 )
    {
        gpib_local( hp5352b.device );
        hp5352b.device = -1;
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
        return vars_push( FLOAT_VAR, HP5352B_TEST_FREQUENCY );

    return vars_push( FLOAT_VAR, hp5352b_get_freq( ) );
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
		print( FATAL, "Frequency resolution can only be set, not queried.\n" );
		THROW( EXCEPTION );
	}

	resolution = get_double( v, "frequency resolution" );

	too_many_arguments( v );

	if ( resolution < 0.99 * res[ 0 ] )
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

		if ( resolution > 1.01 * res[ 6 ] )
		{
			print( SEVERE, "Resolution too low, switching to 1 MHz\n" );
			resolution = res[ 6 ];
			i = 6;
		}

		if ( fabs( resolution - res[ i ] ) > 0.01 * resolution )
			print( SEVERE, "Requested resolution of %f can't be set exactly, "
				   "using %.3f Hz  instead.\n", resolution, res[ i ] );
	}

	hp5352b.res = i;
	hp5352b.is_res = SET;

	if ( FSC2_MODE == EXPERIMENT )
		hp5352b_set_resolution( i );

	return vars_push( FLOAT_VAR, res[ i ] );
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
            hp5352b_command( cmd );
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
hp5352b_init( const char * name )
{
    if ( gpib_init_device( name, &hp5352b.device ) == FAILURE )
        return FAIL;

    /* Tell device to use internal sample rate and to output data only if
       addressed as talker (don't lock the keyboard, the user is supposed
       to do settings at the front panel) */

    if ( gpib_write( hp5352b.device, "CLR\r\n", 5 ) == FAILURE )
        return FAIL;

    if ( gpib_write( hp5352b.device, "SAMPLE,FAST\r\n", 13 ) == FAILURE )
        return FAIL;

    if ( hp5352b.is_res )
        hp5352b_set_resolution( hp5352b.res );

    return OK;
}


/*---------------------------------------------------*
 * To get a new frequency just read from the device,
 * setting it up as talker makes it send a value.
 * The device sometimes seems to send an single 'N'
 * character instead of a string with the frequency,
 * in that case give it another chance...
 *---------------------------------------------------*/

static double
hp5352b_get_freq( void )
{
    char buf[ 25 ];
    long len;
    double res;
    char *ep;


	len = 24;
    if ( gpib_read( hp5352b.device, buf, &len ) == FAILURE )
    {
        print( FATAL, "Communication with device failed: %s.\n",
			   gpib_last_error( ) );
        THROW( EXCEPTION );
    }

	if ( len != 24 )
    {
        print( FATAL, "Less data received from device than requested.\n" );
        THROW( EXCEPTION );
    }

    if ( buf[ 2 ] != '\r' || buf[ 23 ] != '\n' )
    {
        print( FATAL, "Device returned unexpected data.\n" );
        THROW( EXCEPTION );
    }

    buf[ 22 ] = '\0';
    errno = 0;
    res = strtod( buf, &ep );

    if ( *ep || ( ( res == HUGE_VAL || res == -HUGE_VAL ) && errno == ERANGE ) )
    {
        print( FATAL, "Device returned unexpected data.\n" );
        THROW( EXCEPTION );
    }

    return res;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static double
hp5352b_set_resolution( int res )
{
	char cmd[ ] = "RESOL,*\r\n";


	fsc2_assert( res >= 0 && res <= 6 );

	cmd[ 2 ] = res + '0';
    if ( gpib_write( hp5352b.device, cmd, 3 ) == FAILURE )
    {
        print( FATAL, "Communication with device failed: %s.\n",
			   gpib_last_error( ) );
        THROW( EXCEPTION );
    }
    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
hp5352b_command( const char * cmd )
{
    if ( gpib_write( hp5352b.device, cmd, strlen( cmd ) ) == FAILURE )
    {
        print( FATAL, "Communication with device failed: %s.\n",
			   gpib_last_error( ) );
        THROW( EXCEPTION );
    }

    return OK;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
