/* -*-C-*-
 *  $Id$
 * 
 *  Copyright (C) 1999-2004 Jens Thoms Toerring
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


#include "rs_sml01.h"


static void rs_sml01_initial_mod_setup( void );
#if defined WITH_PULSE_MODULATION
static bool rs_sml01_get_pulse_state( void );
static bool rs_sml01_get_pulse_trig_slope( void );
static double rs_sml01_get_pulse_width( void );
static double rs_sml01_get_pulse_delay( void );
#endif
static void rs_sml01_comm_failure( void );
static bool rs_sml01_talk( const char *cmd, char *reply, long *length );


static int dev_handle;

/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

bool rs_sml01_init( const char *name )
{
	double att;
	int i;


	if ( gpib_init_device( name, &dev_handle ) == FAILURE )
        return FAIL;

	/* Set up default settings */

	rs_sml01_command( "CORR:STAT OFF\n" );
	rs_sml01_command( "FREQ:MODE CW\n" );
	rs_sml01_command( "FREQ:ERAN OFF\n" );
	rs_sml01_command( "FREQ:OFFS 0\n" );
	rs_sml01_command( "POW:MODE CW\n" );
	rs_sml01_command( "POW:OFFS 0\n" );
	rs_sml01_command( "POW:ALC ON\n" );
	rs_sml01_command( "OUTP:AMOD AUTO\n" );
#if defined WITH_PULSE_MODULATION
	rs_sml01_command( "PULM:SOUR INT\n" );
	rs_sml01_command( "PULM:POL NORM\n" );
	rs_sml01_command( "PULS:DOUB:STAT OFF\n" );
#endif

	/* Figure out the current frequency if it's not going to be set */

	if ( ! rs_sml01.freq_is_set )
		rs_sml01.freq = rs_sml01_get_frequency( );
	else
		rs_sml01_set_frequency( rs_sml01.freq );

	/* Set or get the current attenuation */

	if ( rs_sml01.attenuation_is_set )
	{
		if ( rs_sml01.use_table )
		{
			att =   rs_sml01.attenuation
				  + rs_sml01_get_att_from_table( rs_sml01.freq )
				  - rs_sml01.att_at_ref_freq;
			if ( att < MAX_ATTEN )
			{
				print( SEVERE, "Attenuation range is too low, using "
					   "%.1f dBm instead of %.1f dBm.\n", MAX_ATTEN, att );
				att = MAX_ATTEN;
			}
			if ( att > rs_sml01.min_attenuation )
			{
				print( SEVERE, "Attenuation range is too high, using "
					   "%.1f dBm instead of %.1f dBm.\n",
					   rs_sml01.min_attenuation, att );
				att = rs_sml01.min_attenuation;
			}
		}
		else
			att = rs_sml01.attenuation;

		rs_sml01_set_attenuation( att );
	}
	else
		rs_sml01.attenuation = rs_sml01_get_attenuation( );

	/* Now we set the modulation type if it has been set, otherwise ask the
	   synthesizer for its currents setting */

	if ( rs_sml01.mod_type_is_set )
	{
		rs_sml01_check_mod_ampl( rs_sml01.freq );
		rs_sml01_set_mod_type( rs_sml01.mod_type );
	}
	else
		rs_sml01_initial_mod_setup( );

	/* Set source and amplitude for each modulation type as far as it's set */

	for ( i = 0; i < NUM_MOD_TYPES - 1; i++ )
	{
		if ( rs_sml01.mod_source_is_set[ i ] )
			rs_sml01_set_mod_source( i, rs_sml01.mod_source[ i ],
									 rs_sml01.mod_freq[ i ] );
		else
		{
			rs_sml01.mod_source[ i ] =
						   rs_sml01_get_mod_source( i, rs_sml01.mod_freq + i );
			rs_sml01.mod_source_is_set[ i ] = SET;
		}

		if ( rs_sml01.mod_ampl_is_set[ i ] )
			rs_sml01_set_mod_ampl( i, rs_sml01.mod_ampl[ i ] );
		else
		{
			rs_sml01.mod_ampl[ i ] = rs_sml01_get_mod_ampl( i );
			rs_sml01.mod_ampl_is_set[ i ] = SET;
		}
	}

#if defined WITH_PULSE_MODULATION
	if ( rs_sml01.pulse_trig_slope_is_set )
		rs_sml01_set_pulse_trig_slope( rs_sml01.pulse_trig_slope );
	else
		rs_sml01.pulse_trig_slope = rs_sml01_get_pulse_trig_slope( );

	if ( rs_sml01.pulse_width_is_set )
		rs_sml01_set_pulse_width( rs_sml01.pulse_width );
	else
		rs_sml01.pulse_width = rs_sml01_get_pulse_width( );

	if ( rs_sml01.pulse_delay_is_set )
		rs_sml01_set_pulse_delay( rs_sml01.pulse_delay );
	else
		rs_sml01.pulse_delay = rs_sml01_get_pulse_delay( );

	if ( rs_sml01.pulse_mode_state_is_set )
		rs_sml01_set_pulse_state( rs_sml01.pulse_mode_state );
	else
		rs_sml01.pulse_mode_state = rs_sml01_get_pulse_state( );
#endif

	rs_sml01_set_output_state( rs_sml01.state );

	return OK;
}


/*--------------------------------------------------------------*
 * Function figures out which types of modulation are currently
 * switched on. If none or only one is on nothing is changed,
 * but if two or more modulations are on all are switched off.
 * Afterwards the bandwidth for all types of modulation is
 * switched to STANDARD.
 *--------------------------------------------------------------*/

static void rs_sml01_initial_mod_setup( void )
{
	unsigned flags = 0;
	const char *types[ ] = { "FM", "AM", "PM" };
	char cmd[ 100 ];
	char buffer[ 100 ];
	long length;
	unsigned int i;


	for ( i = 0; i < NUM_MOD_TYPES - 1; i++ )
	{
		length = 100;
		sprintf( cmd, "%s:STAT?\n", types[ i ] );
		rs_sml01_talk( cmd, buffer, &length );

		if ( buffer[ 0 ] == '1' )
			flags |= 1 << i;
	}

	switch ( flags )
	{
		case 0 :
			rs_sml01.mod_type_is_set = UNSET;
			rs_sml01.mod_type = MOD_TYPE_OFF;
			break;

		case 1 :
			rs_sml01.mod_type_is_set = SET;
			rs_sml01.mod_type = MOD_TYPE_FM;
			rs_sml01_check_mod_ampl( rs_sml01.freq );
			break;

		case 2 :
			rs_sml01.mod_type_is_set = SET;
			rs_sml01.mod_type = MOD_TYPE_AM;
			break;

		case 4 :
			rs_sml01.mod_type_is_set = SET;
			rs_sml01.mod_type = MOD_TYPE_PM;
			rs_sml01_check_mod_ampl( rs_sml01.freq );
			break;

		default :
			for ( i = 0; i < NUM_MOD_TYPES - 1; i++ )
			{
				if ( ! ( flags & ( 1 << i ) ) )
					continue;
				sprintf( cmd, "%s:STAT OFF", types[ i ] );
				rs_sml01_talk( cmd, buffer, &length );
			}
			rs_sml01.mod_type_is_set = UNSET;
			rs_sml01.mod_type = MOD_TYPE_OFF;
	}

	for ( i = 0; i < NUM_MOD_TYPES - 1; i++ )
	{
		if ( i == MOD_TYPE_AM )
			continue;
		sprintf( buffer, "%s:BAND STAN\n", types[ i ] );
		rs_sml01_command( buffer );
	}
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void rs_sml01_finished( void )
{
	gpib_local( dev_handle );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

bool rs_sml01_set_output_state( bool state )
{
	char cmd[ 100 ];


	sprintf( cmd, "OUTP %s\n", state ? "ON" : "OFF" );
	rs_sml01_command( cmd );

	return state;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

bool rs_sml01_get_output_state( void )
{
	char buffer[ 10 ];
	long length = 10;


	rs_sml01_talk( "OUTP?\n", buffer, &length );
	return buffer[ 0 ] == '1';
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

double rs_sml01_set_frequency( double freq )
{
	char cmd[ 100 ];


	fsc2_assert( freq >= MIN_FREQ && freq <= MAX_FREQ );

	sprintf( cmd, "FREQ:CW %.0f\n", freq );
	rs_sml01_command( cmd );

	return freq;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

double rs_sml01_get_frequency( void )
{
	char buffer[ 100 ];
	long length = 100;


	rs_sml01_talk( "FREQ:CW?\n", buffer, &length );
	buffer[ length - 1 ] = '\0';
	return T_atod( buffer );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

double rs_sml01_set_attenuation( double att )
{
	char cmd[ 100 ];


	fsc2_assert( att >= MAX_ATTEN && att <= rs_sml01.min_attenuation );

	sprintf( cmd, "POW %6.1f\n", att );
	rs_sml01_command( cmd );

	return att;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

double rs_sml01_get_attenuation( void )
{
	char buffer[ 100 ];
	long length = 100;

	rs_sml01_talk( "POW?\n", buffer, &length );
	buffer[ length - 1 ] = '\0';
	return T_atod( buffer );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

int rs_sml01_set_mod_type( int type )
{
	switch ( type )
	{
		case MOD_TYPE_FM :
			rs_sml01_command( "AM:STAT OFF;PM:STAT OFF;FM:STAT ON\n" );
			break;
			
		case MOD_TYPE_AM :
			rs_sml01_command( "FM:STAT OFF;PM:STAT OFF;AM:STAT ON\n" );
			break;

		case MOD_TYPE_PM :
			rs_sml01_command( "AM:STAT OFF;FM:STAT OFF;PM STAT ON\n" );
			break;

		case MOD_TYPE_OFF :
			rs_sml01_command( "AM:STAT OFF;FM:STAT OFF;PM STAT OFF\n" );
			break;

		default :                         /* should never happen... */
			fsc2_assert( 1 == 0 );
	}

	return type;
}


/*--------------------------------------------------------------*
 * Function returns a number between 0 and (NUM_MOD_TYPES - 2)
 * indicating the modulation type that is currently switched on
 * or -1 if none is switched on.
 *--------------------------------------------------------------*/

int rs_sml01_get_mod_type( void )
{
	const char *types[ ] = { "FM", "AM", "PM" };
	char cmd[ 100 ];
	char buffer[ 100 ];
	long length;
	int i;


	for ( i = 0; i < NUM_MOD_TYPES - 1; i++ )
	{
		length = 100;
		sprintf( cmd, "%s:STAT?\n", types[ i ] );
		rs_sml01_talk( cmd, buffer, &length );

		if ( buffer[ 0 ] == '1' )
			return i;
	}

	return UNDEFINED;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

int rs_sml01_set_mod_source( int type, int source, double freq )
{
	const char *types[ ] = { "FM", "AM", "PM" };
	char cmd[ 100 ];


	fsc2_assert( type >= 0 && type < NUM_MOD_TYPES );
	fsc2_assert( source >= 0 && source < NUM_MOD_SOURCES );

	switch( source )
	{
		case MOD_SOURCE_AC :
			sprintf( cmd, "%s:SOUR EXT;COUP AC\n", types[ type ] );
			break;

		case MOD_SOURCE_DC :
			sprintf( cmd, "%s:SOUR EXT;COUP DC\n", types[ type ] );
			break;

		case MOD_SOURCE_INT :
			sprintf( cmd, "%s:SOUR INT;FREQ %f\n", types[ type ], freq );
			break;

		default :                         /* this can never happen... */
			fsc2_assert( 1 == 0 );
	}

	rs_sml01_command( cmd );

	return source;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

int rs_sml01_get_mod_source( int type, double *freq )
{
	const char *types[ ] = { "FM", "AM", "PM" };
	char cmd[ 100 ];
	char buffer[ 100 ];
	long length;
	int source;


	fsc2_assert( type >= 0 && type < NUM_MOD_TYPES );

	sprintf( cmd, "%s:SOUR?\n", types[ type ] );
	length = 100;
	rs_sml01_talk( cmd, buffer, &length );

	length = 100;
	if ( ! strncmp( buffer, "INT", 3 ) )
	{
		source = MOD_SOURCE_INT;
		sprintf( cmd, "%s:INT:FREQ?\n", types[ type ] );
		rs_sml01_talk( cmd, buffer, &length );
		buffer[ length - 1 ] = '\0';
		*freq = T_atod( buffer );
	}
	else if ( ! strncmp( buffer, "EXT", 3 ) )
	{
		sprintf( cmd, "%s:EXT:COUP?\n", types[ type ] );
		rs_sml01_talk( cmd, buffer, &length );
		source = buffer[ 0 ] == 'A' ? MOD_SOURCE_AC : MOD_SOURCE_DC;
	}
	else
	{
		print( FATAL, "Device returned unexpected data.\n" );
		THROW( EXCEPTION );
	}

	return source;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

double rs_sml01_set_mod_ampl( int type, double ampl )
{
	char cmd[ 100 ];


	fsc2_assert( type >= 0 && type < NUM_MOD_TYPES );

	switch ( rs_sml01.mod_type )
	{
		case MOD_TYPE_FM :
			sprintf( cmd, "FM %fHz\n", ampl );
			break;

		case MOD_TYPE_AM :
			sprintf( cmd, "AM %fPCT\n", ampl );
			break;

		case MOD_TYPE_PM :
			sprintf( cmd, "PM %fRAD\n", ampl );
			break;

		default :                         /* this can never happen... */
			fsc2_assert( 1 == 0 );
	}

	rs_sml01_command( cmd );

	return ampl;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

double rs_sml01_get_mod_ampl( int type )
{
	const char *cmds[ ] = { "FM?\n", "AM?\n", "PM?\n" };
	char buffer[ 100 ];
	long length = 100;


	fsc2_assert( type >= 0 && type < NUM_MOD_TYPES );

	rs_sml01_talk( cmds[ type ], buffer, &length );
	buffer[ length - 1 ] = '\0';
	return T_atod( buffer );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

#if defined WITH_PULSE_MODULATION

void rs_sml01_set_pulse_state( bool state )
{
	char cmd[ 100 ] = "PULM:STAT ";

	if ( state )
		strcat( cmd, "ON\n" );
	else
		strcat( cmd, "OFF\n" );

	rs_sml01_command( cmd );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static bool rs_sml01_get_pulse_state( void )
{
	char buffer[ 20 ];
	long length = 20;


	rs_sml01_talk( "PULM:STAT?\n", buffer, &length );
	return buffer[ 0 ] == '1';
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void rs_sml01_set_pulse_trig_slope( bool state )
{
	char cmd[ 100 ] = ":TRIG:PULS:SLOP ";


	if ( state == SLOPE_RAISE )
		strcat( cmd, "POS\n" );
	else
		strcat( cmd, "NEG\n" );

	rs_sml01_command( cmd );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static bool rs_sml01_get_pulse_trig_slope( void )
{
	char buffer[ 20 ];
	long length = 20;


	rs_sml01_talk( ":TRIG:PULS:SLOP?\n", buffer, &length );
	return buffer[ 0 ] == 'P' ? SLOPE_RAISE : SLOPE_FALL;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void rs_sml01_set_pulse_width( double width )
{
	char cmd[ 100 ];


	sprintf( cmd, "PULS:WIDT %s\n", rs_sml01_pretty_print( width ) );
	rs_sml01_command( cmd );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static double rs_sml01_get_pulse_width( void )
{
	char buffer[ 100 ];
	long length = 100;


	rs_sml01_talk( "PULS:WIDT?\n", buffer, &length );
	buffer[ length - 1 ] = '\0';
	return T_atod( buffer );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void rs_sml01_set_pulse_delay( double delay )
{
	char cmd[ 100 ];


	sprintf( cmd, "PULS:DEL %s\n", rs_sml01_pretty_print( delay ) );
	rs_sml01_command( cmd );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static double rs_sml01_get_pulse_delay( void )
{
	char buffer[ 100 ];
	long length = 100;


	rs_sml01_talk( "PULS:DEL?\n", buffer, &length );
	buffer[ length - 1 ] = '\0';
	return T_atod( buffer );
}

#endif /* WITH_PULSE_MODULATION */


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static void rs_sml01_comm_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

bool rs_sml01_command( const char *cmd )
{
	if ( gpib_write( dev_handle, cmd, strlen( cmd ) ) == FAILURE )
		rs_sml01_comm_failure( );
	return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool rs_sml01_talk( const char *cmd, char *reply, long *length )
{
	if ( gpib_write( dev_handle, cmd, strlen( cmd ) ) == FAILURE ||
		 gpib_read( dev_handle, reply, length ) == FAILURE )
		rs_sml01_comm_failure( );
	return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
