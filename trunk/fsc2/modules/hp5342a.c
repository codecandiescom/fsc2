/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
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


/*
 *   The HP5342A is a very old device, it's from 1979, i.e. even pre-dating
 *   the GPIB standard. Its programming capabilities are rather limited,
 *   especially one can't find out about the settings of the device and
 *   one either has to set all parameters via the program or none at all.
 *   Thus, to keep things simple, all settings must be made via the front
 *   panel and the only function that is supported is querying the measured
 *   frequency.
 * 
 *   The device is set to a state where it measures new frequencies with an
 *   internal sample rate (which can be adjusted via a knob at the front
 *   panel) and to output data only when it becomes addressed as talker.
 */


#include "fsc2_module.h"
#include "gpib_if.h"


/* Include configuration information for the device */

#include "hp5342a.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


int hp5342a_init_hook(       void );
int hp5342a_exp_hook(        void );
int hp5342a_end_of_exp_hook( void );


Var_T * freq_counter_name(       Var_T * v );
Var_T * freq_counter_measure(    Var_T * v );
Var_T * freq_counter_resolution( Var_T * v );
Var_T * freq_counter_command(    Var_T * v );


static bool hp5342a_init( const char * name );

static double hp5342a_get_freq( void );

static double hp5342a_set_resolution( int );
static bool hp5342a_command( const char * );


static struct {
    int device;
	int res;
	bool is_res;
} hp5342a;


/* Define the frequency that the driver returns during the test run */

#define HP5342A_TEST_FREQUENCY  2.6e9



/**********************************************************/
/*                                                        */
/*                  hook functions                        */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
hp5342a_init_hook( void )
{
    /* Set global variable to indicate that GPIB bus is needed */

    Need_GPIB = SET;

    hp5342a.device = -1;
	hp5342a.is_res = UNSET;
    return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
hp5342a_exp_hook( void )
{
    if ( ! hp5342a_init( device_name ) )
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
hp5342a_end_of_exp_hook( void )
{
    hp5342a.device = -1;
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
freq_counter_measure( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, HP5342A_TEST_FREQUENCY );

    return vars_push( FLOAT_VAR, hp5342a_get_freq( ) );
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

	hp5342a.res = i;
	hp5342a.is_res = SET;

	if ( FSC2_MODE == EXPERIMENT )
		hp5342a_set_resolution( i );

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
            hp5342a_command( cmd );
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



/**************************************************/
/*                                                */
/*       Internal (non-exported) functions        */
/*                                                */
/**************************************************/

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static bool
hp5342a_init( const char * name )
{
    if ( gpib_init_device( name, &hp5342a.device ) == FAILURE )
        return FAIL;

    /* Tell device to use internal sample rate and to output data only if
       addressed as talker(don't lock the keyboard, the user is supposed
       to do settings at the front panel) */

    if ( gpib_write( hp5342a.device, "ST1", 3 ) == FAILURE )
        return FAIL;

    if ( gpib_write( hp5342a.device, "T2", 2 ) == FAILURE )
        return FAIL;

    if ( hp5342a.is_res )
        hp5342a_set_resolution( hp5342a.res );

    return OK;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static double
hp5342a_get_freq( void )
{
    char buf[ 22 ];
    long len = 22;

    if (    gpib_read( hp5342a.device, buf, &len ) == FAILURE
         || len != 22
         || strncmp( buf, " F  ", 4 ) )
    {
        print( FATAL, "Communication with device failed.\n" );
        THROW( EXCEPTION );
    }

    buf[ 20 ] = '\0';
    return T_atod( buf + 4 );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static double
hp5342a_set_resolution( int res )
{
	char cmd[ 3 ] = "SR";


	fsc2_assert( res >= 0 && res <= 6 );

	cmd[ 2 ] = '3' + res;
    if ( gpib_write( hp5342a.device, cmd, 3 ) == FAILURE )
    {
        print( FATAL, "Communication with device failed.\n" );
        THROW( EXCEPTION );
    }
    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
hp5342a_command( const char * cmd )
{
    if ( gpib_write( hp5342a.device, cmd, strlen( cmd ) ) == FAILURE )
    {
        print( FATAL, "Communication with device failed.\n" );
        THROW( EXCEPTION );
    }

    return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
