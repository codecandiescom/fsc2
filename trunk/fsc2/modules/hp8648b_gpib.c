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
  I had some rather strange probems with this device. For example, when simply
  just sweeping up the output frequency I nearly never ended up with the
  expected end frequency but a bit below eventhough all commands got send
  correctly. Obviously, some kind of backlog of commands had built up. To get
  rid of this (and also some spurious break-downs in the communication) I now
  check the operation complete after each setting of the synthesizer.
*/



#include "hp8648b.h"


static void hp8648b_comm_failure( void );
static void hp8648b_check_complete( void );
static bool hp8648b_talk( const char *cmd, char *reply, long *length );


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

bool hp8648b_init( const char *name )
{
	double att;
	char buffer[ 10 ];
	long length = 10;
	int i;


	if ( gpib_init_device( name, &hp8648b.device ) == FAILURE )
        return FAIL;

	hp8648b_talk( "*OPC?\n", buffer, &length );

	/* If frequency and attenuation need to be set do it now, otherwise get
	   frequency and attenuation set at the synthesizer and store it */

	if ( hp8648b.freq_is_set )
		hp8648b_set_frequency( hp8648b.freq );
	else
		hp8648b.freq = hp8648b_get_frequency( );

	if ( hp8648b.attenuation_is_set )
	{
		if ( hp8648b.use_table )
		{
			att =   hp8648b.attenuation
				  + hp8648b_get_att_from_table( hp8648b.freq )
				  - hp8648b.att_at_ref_freq;
			if ( att < MAX_ATTEN )
			{
				print( SEVERE, "Attenuation range is insufficient, using "
					   "%f db instead of %f db.\n", MAX_ATTEN, att );
				att = MAX_ATTEN;
			}
			if ( att > hp8648b.min_attenuation )
			{
				print( SEVERE, "Attenuation range is insufficient, using "
					   "%f db instead of %f db.\n",
					   hp8648b.min_attenuation, att );
				att = hp8648b.min_attenuation;
			}
		}
		else
			att = hp8648b.attenuation;

		hp8648b_set_attenuation( att );
	}
	else
		hp8648b.attenuation = hp8648b_get_attenuation( );

	hp8648b_set_output_state( hp8648b.state );

	/* Now we set the modulation type if it has been set, otherwise ask the
	   synthesizer for its currents setting */

	if ( hp8648b.mod_type_is_set )
		hp8648b_set_mod_type( hp8648b.mod_type );
	else
	{
		if ( ( hp8648b.mod_type = hp8648b_get_mod_type( ) ) != UNDEFINED )
			hp8648b.mod_type_is_set = SET;
	}

	/* Set source and amplitude for each modulation type as far as it's set */

	for ( i = 0; i < NUM_MOD_TYPES - 1; i++ )
	{
		if ( hp8648b.mod_source_is_set[ i ] )
			hp8648b_set_mod_source( i, hp8648b.mod_source[ i ] );

		if ( hp8648b.mod_ampl_is_set[ i ] )
			hp8648b_set_mod_ampl( i, hp8648b.mod_ampl[ i ] );
	}

	return OK;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

void hp8648b_finished( void )
{
	gpib_local( hp8648b.device );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

bool hp8648b_set_output_state( bool state )
{
	char cmd[ 100 ];


	sprintf( cmd, "OUTP:STAT %s\n", state ? "ON" : "OFF" );
	hp8648b_command( cmd );
	hp8648b_check_complete( );

	return state;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

bool hp8648b_get_output_state( void )
{
	char buffer[ 10 ];
	long length = 10;


	hp8648b_talk( "OUTP:STAT?\n", buffer, &length );
	return buffer[ 0 ] == '1';
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8648b_set_frequency( double freq )
{
	char cmd[ 100 ];


	fsc2_assert( freq >= MIN_FREQ && freq <= MAX_FREQ );

	sprintf( cmd, "FREQ:CW %.0f\n", freq );
	hp8648b_command( cmd );
	hp8648b_check_complete( );

	return freq;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8648b_get_frequency( void )
{
	char buffer[ 100 ];
	long length = 100;


	hp8648b_talk( "FREQ:CW?\n", buffer, &length );
	return T_atod( buffer );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8648b_set_attenuation( double att )
{
	char cmd[ 100 ];


	fsc2_assert( att >= MAX_ATTEN && att <= hp8648b.min_attenuation );

	sprintf( cmd, "POW:AMPL %6.1f\n", att );
	hp8648b_command( cmd );
	hp8648b_check_complete( );

	return att;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8648b_get_attenuation( void )
{
	char buffer[ 100 ];
	long length = 100;

	hp8648b_talk( "POW:AMPL?\n", buffer, &length );
	return T_atod( buffer );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

int hp8648b_set_mod_type( int type )
{
	char cmd[ 100 ];
	const char *types[ ] = { "FM", "AM", "PM", "OFF" };
	int i;


	fsc2_assert( type >= 0 && type < NUM_MOD_TYPES );

	/* The manual is not really clear about this but it looks as if we
	   have to make sure that only one modulation type is switched on... */

	for ( i = 0; i < NUM_MOD_TYPES - 1; i++ )
	{
		if ( i == type )
			continue;
		sprintf( cmd, "%s:STAT OFF\n", types[ i ] );
		hp8648b_command( cmd );
	}

	if ( type != MOD_TYPE_OFF )
	{
		sprintf( cmd, "%s:STAT ON\n", types[ type ] );
		hp8648b_command( cmd );
		hp8648b_check_complete( );
	}

	return type;
}


/*--------------------------------------------------------------*/
/* Function returns a number between 0 and (NUM_MOD_TYPES - 1)  */
/* indicating the modulation type that is currently switched on */
/* or -1 if none is switched on.                                */
/*--------------------------------------------------------------*/

int hp8648b_get_mod_type( void )
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
		hp8648b_talk( cmd, buffer, &length );

		if ( buffer[ 0 ] == '1' )
			return i;
	}

	return UNDEFINED;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

int hp8648b_set_mod_source( int type, int source )
{
	const char *types[ ] = { "FM", "AM", "PM" };
	char cmd1[ 100 ], cmd2[ 100 ];


	fsc2_assert( type >= 0 && type < NUM_MOD_TYPES );
	fsc2_assert( source >= 0 && source < NUM_MOD_SOURCES );

	/* Neither AM nor phase modulation allows external DC as modulation
	   source */

	if ( type != MOD_TYPE_FM && source == MOD_SOURCE_DC )
	{
		print( SEVERE, "Modulation source \"%s\" can't be used for %s "
			   "modulation, using \"AC\" instead.\n",
			   mod_sources[ source ], mod_types[ type ] );
		source = MOD_SOURCE_AC;
	}

	sprintf( cmd1, "%s:SOUR ", types[ type ] );
	switch( source )
	{
		case MOD_SOURCE_AC :
			strcat( cmd1, "EXT\n" );
			sprintf( cmd2, "%s:EXT:COUP AC\n", types[ type ] );
			break;

		case MOD_SOURCE_DC :
			strcat( cmd1, "EXT\n" );
			sprintf( cmd2, "%s:EXT:COUP DC\n", types[ type ] );
			break;

		case MOD_SOURCE_1k :
			strcat( cmd1, "INT\n" );
			sprintf( cmd2, "%s:INT:FREQ 1 KHZ\n", types[ type ] );
			break;

		case MOD_SOURCE_400 :
			strcat( cmd1, "INT\n" );
			sprintf( cmd2, "%s:INT:FREQ 400 HZ\n", types[ type ] );
			break;

		default :                         /* this can never happen... */
			fsc2_assert( 1 == 0 );
	}

	hp8648b_command( cmd1 );
	hp8648b_command( cmd2 );
	hp8648b_check_complete( );

	return source;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

int hp8648b_get_mod_source( int type )
{
	const char *types[ ] = { "FM", "AM", "PM" };
	char cmd[ 100 ];
	char buffer[ 100 ];
	long length;
	int source;
	long freq;


	fsc2_assert( type >= 0 && type < NUM_MOD_TYPES );

	sprintf( cmd, "%s:SOUR?\n", types[ type ] );
	length = 100;
	hp8648b_talk( cmd, buffer, &length );

	source = buffer[ 0 ] == 'I' ? 0 : 1;

	length = 100;
	if ( source == 0 )
	{
		sprintf( cmd, "%s:INT:FREQ?\n", types[ type ] );
		hp8648b_talk( cmd, buffer, &length );
		freq = lrnd( T_atod ( buffer ) );
		source = freq == 400 ? MOD_SOURCE_1k : MOD_SOURCE_400;
	}
	else
	{
		sprintf( cmd, "%s:EXT:COUP?\n", types[ type ] );
		hp8648b_talk( cmd, buffer, &length );
		source = buffer[ 0 ] == 'A' ? MOD_SOURCE_AC : MOD_SOURCE_DC;
	}

	return source;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8648b_set_mod_ampl( int type, double ampl )
{
	char cmd[ 100 ];


	fsc2_assert( type >= 0 && type < NUM_MOD_TYPES );

	switch ( hp8648b.mod_type )
	{
		case MOD_TYPE_FM :
			sprintf( cmd, "FM:DEV %ld HZ\n", 10 * lrnd( 0.1 * ampl ) );
			break;

		case MOD_TYPE_AM :
			sprintf( cmd, "AM:DEPT %.1f PCT\n", ampl );
			break;

		case MOD_TYPE_PHASE :
			sprintf( cmd, "PM:DEV %.*f RAD\n", ampl < 9.95 ? 2 : 1, ampl );
			break;

		default :                         /* this can never happen... */
			fsc2_assert( 1 == 0 );
	}

	hp8648b_command( cmd );
	hp8648b_check_complete( );

	return ampl;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8648b_get_mod_ampl( int type )
{
	const char *cmds[ ] = { "FM:DEV?\n", "AM:DEPT?\n", "PM:DEV?\n" };
	char buffer[ 100 ];
	long length = 100;


	fsc2_assert( type >= 0 && type < NUM_MOD_TYPES );

	hp8648b_talk( cmds[ type ], buffer, &length );
	return T_atod( buffer );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static void hp8648b_comm_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static void hp8648b_check_complete( void )
{
	char buffer[ 10 ];
	long length = 10;


	do {
		hp8648b_talk( "*OPC?\n", buffer, &length );
	} while ( buffer[ 1 ] != '1' );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

bool hp8648b_command( const char *cmd )
{
	if ( gpib_write( hp8648b.device, cmd, strlen( cmd ) ) == FAILURE )
		hp8648b_comm_failure( );
	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool hp8648b_talk( const char *cmd, char *reply, long *length )
{
	if ( gpib_write( hp8648b.device, cmd, strlen( cmd ) ) == FAILURE ||
		 gpib_read( hp8648b.device, reply, length ) == FAILURE )
		hp8648b_comm_failure( );
	return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
