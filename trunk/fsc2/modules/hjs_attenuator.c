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


#include "hjs_attenuator.h"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


HJS_ATTENUATOR hjs_attenuator, hjs_attenuator_stored;


static bool hjs_attenuator_serial_init( void );
static void hjs_attenuator_set_attenuation( long new_step );
static void hjs_attenuator_comm_failure( void );
static FILE *hjs_attenuator_find_table( char **name );
static FILE *hjs_attenuator_open_table( char *name );
static long hjs_attenuator_att_to_step( double att );


/*------------------------------------------------------*/
/* Function called when the module has just been loaded */
/*------------------------------------------------------*/

int hjs_attenuator_init_hook( void )
{
	fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

	hjs_attenuator.is_open       = UNSET;
	hjs_attenuator.is_step       = UNSET;
	hjs_attenuator.table_file    = NULL;
	hjs_attenuator.att_table     = NULL;
	hjs_attenuator.att_table_len = 0;

	vars_pop( mw_attenuator_use_table( NULL ) );

	return 1;
}


/*----------------------------------------------*/
/* Function called at the start of the test run */
/*----------------------------------------------*/

int hjs_attenuator_test_hook( void )
{
	hjs_attenuator_stored = hjs_attenuator;
	return 1;
}


/*-----------------------------------------------*/
/* Function called at the start of an experiment */
/*-----------------------------------------------*/

int hjs_attenuator_exp_hook( void )
{
	/* Restore state from before the start of the test run (or the
	   last experiment) */

	hjs_attenuator = hjs_attenuator_stored;

	if ( ! ( hjs_attenuator.is_open = hjs_attenuator_serial_init( ) ) )
		hjs_attenuator_comm_failure( );
	
	return 1;
}


/*---------------------------------------------*/
/* Function called at the end of an experiment */
/*---------------------------------------------*/

int hjs_attenuator_end_of_exp_hook( void )
{
	if ( hjs_attenuator.is_open )
		fsc2_serial_close( SERIAL_PORT );

	hjs_attenuator.is_open = UNSET;
	return 1;
}


/*------------------------------------------------------*/
/* Function called just before the module gets unloaded */
/*------------------------------------------------------*/

void hjs_attenuator_exit_hook( void )
{
	/* Get rid of the data from the table file */

	if ( hjs_attenuator.att_table )
		hjs_attenuator.att_table =
					  HJS_ATT_TABLE_ENTRY_P T_free( hjs_attenuator.att_table );

	/* This shouldn't be necessary, but to make 100% sure the device file
	   is closed we do it anyway */

	if ( hjs_attenuator.is_open )
		fsc2_serial_close( SERIAL_PORT );
	hjs_attenuator.is_open = UNSET;
}


/*----------------------------------------------------------------*/
/* Function returns a string variable with the name of the device */
/*----------------------------------------------------------------*/

Var *mw_attenuator_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

Var *mw_attenuator_use_table( Var *v )
{
	FILE *tfp = NULL;
	char *tfname;


	CLOBBER_PROTECT( tfp );

	/* If a table had already been read in get rid of it */

	if ( hjs_attenuator.att_table != NULL )
	{
		hjs_attenuator.att_table =
					  HJS_ATT_TABLE_ENTRY_P T_free( hjs_attenuator.att_table );
		hjs_attenuator.att_table_len = 0;
	}

	/* Try to figure out the name of the table file - if no argument is given
	   use the default table file, otherwise use the user supplied file name */

	if ( v == NULL )
	{
		if ( DEFAULT_TABLE_FILE[ 0 ] ==  '/' )
			hjs_attenuator.table_file = T_strdup( DEFAULT_TABLE_FILE );
		else
			hjs_attenuator.table_file = get_string( "%s%s%s", libdir,
													slash( libdir ),
													DEFAULT_TABLE_FILE );

		if ( ( tfp = hjs_attenuator_open_table( hjs_attenuator.table_file ) )
			 == NULL )
		{
			print( FATAL, "Table file '%s' not found.\n",
				   hjs_attenuator.table_file );
			hjs_attenuator.table_file =
									CHAR_P T_free( hjs_attenuator.table_file );
			THROW( EXCEPTION );
		}
	}
	else
	{
		vars_check( v, STR_VAR );

		tfname = T_strdup( v->val.sptr );

		too_many_arguments( v );

		TRY
		{
			tfp = hjs_attenuator_find_table( &tfname );
			hjs_attenuator.table_file = tfname;
			TRY_SUCCESS;
		}
		CATCH( EXCEPTION )
		{
			T_free( tfname );
			RETHROW( );
		}
	}

	/* Now try to read in the table file */

	TRY
	{
		hjs_attenuator_read_table( tfp );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		fclose( tfp );
		hjs_attenuator.table_file = CHAR_P T_free( hjs_attenuator.table_file );
		RETHROW( );
	}

	fclose( tfp );
	hjs_attenuator.table_file = CHAR_P T_free( hjs_attenuator.table_file );

	return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------------------------*/
/* Function that must be called before any other function to set or get */
/* the current attenuation, and then never again. It informs the module */
/* about the attenuation currently set. This information is required to */
/* calculate the number of steps needed to change to a new attenuation. */
/*----------------------------------------------------------------------*/

Var *mw_attenuator_initial_attenuation( Var *v )
{
	hjs_attenuator.att = get_double( v, "attenuation" );
	hjs_attenuator.step = hjs_attenuator_att_to_step( hjs_attenuator.att );
	hjs_attenuator.is_step = SET;
	return vars_push( FLOAT_VAR, hjs_attenuator.att );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

Var *mw_attenuator_attenuation( Var *v )
{
	double att;
	long step;


	if ( ! hjs_attenuator.is_step )
	{
		print( FATAL, "Don't know the current attenuation setting. Always "
			   "call 'mw_attenuator_initial_attenuation()' first!\n" );
		THROW( EXCEPTION );
	}

	if ( v != NULL )
	{
		att = get_double( v, "attenuation" );
		step = hjs_attenuator_att_to_step( att );

		if ( FSC2_MODE == EXPERIMENT )
			hjs_attenuator_set_attenuation( step );

		if ( step - hjs_attenuator.step != 0 )
		{
			hjs_attenuator.att = att;
			hjs_attenuator.step = step;
		}
	}

	return vars_push( FLOAT_VAR, hjs_attenuator.att );
}


/*---------------------------------------------------------------*/
/* Initialization of the serial port the device is connected to. */
/*---------------------------------------------------------------*/

static bool hjs_attenuator_serial_init( void )
{
	if ( ( hjs_attenuator.tio = fsc2_serial_open( SERIAL_PORT, DEVICE_NAME,
								 O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) )
		 == NULL )
		return FAIL;

	/* The device uses 8 bit transfers, no parity and one stop bit (8N1),
	   a baud rate of 9600 and has no control lines (only RTX, DTX and
	   ground are connected internally). Of course, canonical mode can't
	   be used here.*/

	memset( hjs_attenuator.tio, 0, sizeof *hjs_attenuator.tio );
	hjs_attenuator.tio->c_cflag = CS8 | CSIZE | CREAD | CLOCAL;

	cfsetispeed( hjs_attenuator.tio, SERIAL_BAUDRATE );
	cfsetospeed( hjs_attenuator.tio, SERIAL_BAUDRATE );

	fsc2_tcflush( SERIAL_PORT, TCIOFLUSH );
	fsc2_tcsetattr( SERIAL_PORT, TCSANOW, hjs_attenuator.tio );

	return OK;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void hjs_attenuator_set_attenuation( long new_step )
{
	long steps;
	char *cmd;
	ssize_t len;


	/* Figure out how many steps to go */

	steps = new_step - hjs_attenuator.step;

	if ( steps == 0 )
		return;

	cmd = get_string( "@01\n@0A %+ld,300\n", steps );
	len = ( ssize_t ) strlen( cmd );

	if ( fsc2_serial_write( SERIAL_PORT, cmd, ( size_t ) len ) != len )
	{
		T_free( cmd );
		hjs_attenuator_comm_failure( );
	}

	T_free( cmd );
	fsc2_usleep( labs( steps ) * HJS_ATTENTUATOR_WAIT_PER_STEP, SET );
	stop_on_user_request( );

	/* To always reach the end point from the same side we go a bit further
	   up when we come from below and then back down again. */

    if ( steps < 0 )
		return;

	if ( fsc2_serial_write( SERIAL_PORT, "@01\n@0A +50,100\n", 16 ) != 16 )
		hjs_attenuator_comm_failure( );

	fsc2_usleep( 50 * HJS_ATTENTUATOR_WAIT_PER_STEP, UNSET );

	if ( fsc2_serial_write( SERIAL_PORT, "@01\n@0A -50,300\n", 16 ) != 16 )
		hjs_attenuator_comm_failure( );

	fsc2_usleep( 50 * HJS_ATTENTUATOR_WAIT_PER_STEP, UNSET );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void hjs_attenuator_comm_failure( void )
{
	print( FATAL, "Can't access the attenuator.\n" );
	THROW( EXCEPTION );
}


/*----------------------------------------------------------------------*/
/* If the function succeeds it returns a file pointer to the table file */
/* and sets the table_file entry in the device structure to the name of */
/* the table file. Otherwise an exception is thrown. In any case the    */
/* memory used for the file name passed to the function is deallocated. */
/*----------------------------------------------------------------------*/

static FILE *hjs_attenuator_find_table( char **name )
{
	FILE *tfp;
	char *new_name;


	/* Expand a leading tilde to the users home directory */

	if ( ( *name )[ 0 ] == '~' )
	{
		new_name = get_string( "%s%s%s", getenv( "HOME" ),
							   ( *name )[ 1 ] != '/' ? "/" : "", *name + 1 );
		T_free( *name );
		*name = new_name;
	}

	/* Now try to open the file - set table_file entry in the device structure
	   to the name and return the file pointer */

	if ( ( tfp = hjs_attenuator_open_table( *name ) ) != NULL )
	{
		hjs_attenuator.table_file = T_strdup( *name );
		return tfp;
	}

	/* If the file name contains a slash we give up after freeing memory */

	if ( strchr( *name, '/' ) != NULL )
	{
		print( FATAL, "Table file '%s' not found.\n", *name );
		THROW( EXCEPTION );
	}

	/* Last chance: The table file is in the library directory... */

	new_name = get_string( "%s%s%s", libdir, slash( libdir ), *name );
	T_free( *name );
	*name = new_name;

	if ( ( tfp = hjs_attenuator_open_table( *name ) ) == NULL )
	{
		print( FATAL, "Table file '%s' not found in either the current "
			   "directory or in '%s'.\n", strip_path( *name ), libdir );
		THROW( EXCEPTION );
	}

	return tfp;
}


/*------------------------------------------------------------------*/
/* Tries to open the file with the given name and returns the file  */
/* pointer, returns NULL if file does not exist returns, or throws  */
/* an exception if the file can't be read (either because of prob-  */
/* lems with the permissions or other, unknoen reasons). If opening */
/* the file fails with an exception memory used for its name is     */
/* deallocated.                                                     */
/*------------------------------------------------------------------*/

static FILE *hjs_attenuator_open_table( char *name )
{
	FILE *tfp;


	if ( access( name, R_OK ) == -1 )
	{
		if ( errno == ENOENT )       /* file not found */
			return NULL;

		print( FATAL, "No read permission for table file '%s'.\n", name );
		T_free( name );
		THROW( EXCEPTION );
	}

	if ( ( tfp = fopen( name, "r" ) ) == NULL )
	{
		print( FATAL, "Can't open table file '%s'.\n", name );
		T_free( name );
		THROW( EXCEPTION );
	}

	return tfp;
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

static long hjs_attenuator_att_to_step( double att )
{
	size_t ind;
	size_t max_index;;


	fsc2_assert( hjs_attenuator.att_table != NULL &&
				 hjs_attenuator.att_table_len > 1 );

	if ( att < 0.0 )
	{
		print( FATAL, "Invalid negative attenuation of %.1f dB.\n", att );
		THROW( EXCEPTION );
	}

	if ( att > hjs_attenuator.max_table_att * 1.001 )
	{
		print( FATAL, "Attenuation of %.1f dB is too large, must not be "
			   "larger than %.1f dB.\n", att, hjs_attenuator.max_table_att );
		THROW( EXCEPTION );
	}

	if ( att >= hjs_attenuator.max_table_att )
		return
			hjs_attenuator.att_table[ hjs_attenuator.att_table_len - 1 ].step;

	if ( att < hjs_attenuator.min_table_att * 0.999 )
	{
		print( FATAL, "Attenuation of %.1f dB is too small, must not be "
			   "smaller than %.1f dB.\n", att, hjs_attenuator.min_table_att );
		THROW( EXCEPTION );
	}

	if ( att <= hjs_attenuator.min_table_att )
		return hjs_attenuator.att_table[ 0 ].step;

	ind = hjs_attenuator.att_table_len / 2;
	max_index = hjs_attenuator.att_table_len - 1;

	while( 1 )
	{
		if ( att < hjs_attenuator.att_table[ ind ].att )
		{
			if ( att >= hjs_attenuator.att_table[ ind - 1 ].att )
				return lrnd( hjs_attenuator.att_table[ ind - 1 ].step +
							( hjs_attenuator.att_table[ ind - 1 ].step
							  -  hjs_attenuator.att_table[ ind ].step ) /
							( hjs_attenuator.att_table[ ind - 1 ].att
							  -  hjs_attenuator.att_table[ ind ].att ) *
							( att - hjs_attenuator.att_table[ ind - 1 ].att ) );
					
			max_index = ind;
			ind /= 2;
			continue;
		}

		if ( att < hjs_attenuator.att_table[ ind + 1 ].att )
				return lrnd( hjs_attenuator.att_table[ ind ].step +
							 ( hjs_attenuator.att_table[ ind + 1 ].step
							   -  hjs_attenuator.att_table[ ind ].step ) /
							 ( hjs_attenuator.att_table[ ind + 1 ].att
							   -  hjs_attenuator.att_table[ ind ].att ) *
							 ( att - hjs_attenuator.att_table[ ind ].att ) );

		ind += ( max_index - ind ) / 2;
	}

	return 0;       /* we can't ever end up here */
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
