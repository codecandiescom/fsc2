/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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

/*
   The HP5340A is a very old device, it's from 1973, thus even pre-dating
   the GPIB standard. Its programming capabilities are rather limited,
   especially one can't find out the settings of the device made via the
   front panel and one either has to set all parameters via the program
   or none at all. Thus, to keep things simple, all settings must be made
   via the front panel and the only function that is supported is querying
   the measured frequency.

   The device is set to a state where it measures new frequencies with an
   internal sample rate (which can be adjusted via a knob at the front
   panel) and to output data only when it becomes addressed as talker.
*/


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "hp5340a.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


int hp5340a_init_hook( void );
int hp5340a_exp_hook( void );
int hp5340a_end_of_exp_hook( void );


Var *freq_counter_name( Var *v );
Var *freq_counter_measure( Var *v );
Var *freq_counter_command( Var *v );


static bool hp5340a_init( const char *name );
static double h95340a_get_freq( void );
static bool hp5340a_command( const char *cmd );


static struct {
	int device;
} hp5340a;


/**********************************************************/
/*                                                        */
/*                  hook functions                        */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int hp5340a_init_hook( void )
{
	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	hp5340a.device = -1;
	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int hp5340a_exp_hook( void )
{
	if ( ! hp5340a_init( device_name ) )
	{
		print( FATAL, "Initialization of device failed: %s\n",
			   gpib_error_msg );
		THROW( EXCEPTION );
	}

	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int hp5340a_end_of_exp_hook( void )
{
	hp5340a.device = -1;
	return 1;
}


/**********************************************************/
/*                                                        */
/*          EDL functions                                 */
/*                                                        */
/**********************************************************/

/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *freq_counter_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *freq_counter_measure( Var *v )
{
	v = v;

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, HP5340A_DEFAULT_FREQUENCY );

	return vars_push( FLOAT_VAR, h95340a_get_freq( ) );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *freq_counter_command( Var *v )
{
	static char *cmd;


	cmd = NULL;
	vars_check( v, STR_VAR );
	
	if ( FSC2_MODE == EXPERIMENT )
	{
		TRY
		{
			cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
			hp5340a_command( cmd );
			T_free( cmd );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( cmd );
			RETHROW( );
		}
	}

	return vars_push( INT_VAR, 1 );
}



/**********************************************************/
/*                                                        */
/*       Internal (non-exported) functions                */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static bool hp5340a_init( const char *name )
{
	if ( gpib_init_device( name, &hp5340a.device ) == FAILURE )
        return FAIL;

	/* Tell device to use internal sample rate and to output data only
	   if addressed as talker. */

	if ( gpib_write( hp5340a.device, "JL", 2 ) == FAILURE )
		return FAIL;

	return OK;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static double h95340a_get_freq( void )
{
	char buf[ 16 ];
	long len = 16;

	if ( gpib_read( hp5340a.device, buf, &len ) == FAILURE ||
		 len != 16 )
	{
		print( FATAL, "Communication with device failed.\n" );
		THROW( EXCEPTION );
	}

	if ( buf[ 1 ] != ' ' )
	{
		print( FATAL, "Device detected display overflow.\n" );
		THROW( EXCEPTION );
	}

	buf[ 14 ] = '\0';
	return T_atod( buf + 3 );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool hp5340a_command( const char *cmd )
{
	if ( gpib_write( hp5340a.device, cmd, strlen( cmd ) ) == FAILURE )
	{
		print( FATAL, "Communication with device failed.\n" );
		THROW( EXCEPTION );
	}

	return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
