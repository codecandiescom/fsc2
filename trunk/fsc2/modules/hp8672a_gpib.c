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



#include "hp8672a.h"


static void hp8672a_comm_failure( void );

/*
#define gpib_write( a, b, c ) fprintf( stderr, "%s\n", ( b ) )
#define gpib_init_device( a, b ) 1
*/


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

bool hp8672a_init( const char *name )
{
	double att;
	char buffer[ 10 ];
	long length = 10;


	if ( gpib_init_device( name, &hp8672a.device ) == FAILURE )
        return FAIL;

	if ( gpib_read( hp8672a.device, buffer, &length ) == FAILURE )
		return FAIL;

	if ( buffer[ 0 ] & HP8672A_CRYSTAL_OVEN_COLD )
	{
		print( FATAL, "Synthesizers crystal oven is cold.\n" );
		THROW( EXCEPTION );
	}

	/* Make sure RF output is switched off */

	hp8672a_set_output_state( UNSET );

	HP8672A_INIT = SET;

	/* If frequency and attenuation need to be set do it now, otherwise get
	   frequency and attenuation set at the synthesizer and store it */

	if ( hp8672a.freq_is_set )
		hp8672a_set_frequency( hp8672a.freq );

	if ( hp8672a.attenuation_is_set )
	{
		if ( hp8672a.use_table )
		{
			att =   hp8672a.attenuation
				  + hp8672a_get_att_from_table( hp8672a.freq )
				  - hp8672a.att_at_ref_freq;
			if ( att < MAX_ATTEN )
			{
				print( SEVERE, "Attenuation range is insufficient, using "
					   "%f db instead of %f db.\n", MAX_ATTEN, att );
				att = MAX_ATTEN;
			}
			if ( att > hp8672a.min_attenuation )
			{
				print( SEVERE, "Attenuation range is insufficient, using "
					   "%f db instead of %f db.\n",
					   hp8672a.min_attenuation, att );
				att = hp8672a.min_attenuation;
			}
		}
		else
			att = hp8672a.attenuation;

		hp8672a_set_attenuation( att );
	}

	/* Set modulation type and amplitude as far as it's set */

	if ( hp8672a.mod_type_is_set )
		hp8672a_set_modulation( );

	/* If user want's RF output... */

	hp8672a_set_output_state( hp8672a.state );

	HP8672A_INIT = UNSET;

	return OK;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

void hp8672a_finished( void )
{
	gpib_local( hp8672a.device );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

bool hp8672a_set_output_state( bool state )
{
	char cmd[ 100 ];


	if ( ! hp8672a.is_10db )
		sprintf( cmd, "O%c\n", state ? '1' : '0' );
	else
		sprintf( cmd, "O%c\n", state ? '3' : '0' );

	if ( gpib_write( hp8672a.device, cmd, 3 ) == FAILURE )
		hp8672a_comm_failure( );

	/* Switching RF ON/OFF takes about 30 ms or 5 ms, respectively */

	fsc2_usleep( state ? 30000 : 5000, UNSET );

	return state;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

bool hp8672a_get_output_state( void )
{
	char buffer[ 10 ];
	long length = 10;


	if ( gpib_read( hp8672a.device, buffer, &length ) == FAILURE )
		hp8672a_comm_failure( );

	return ! ( buffer[ 0 ] & HP8672A_RF_OFF );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8672a_set_frequency( double freq )
{
	char cmd[ 100 ];
	long ifreq, tfreq;


	fsc2_assert( freq >= MIN_FREQ && freq <= MAX_FREQ );

	/* Below 6.2 GHz resolution is 1 KHz, between 6.2 GHz and 12.4 GHz its
	   2 kHz and above we have a resolution of 3 kHz. Without correcting
	   the frequency we'ld get random rounded */

	if ( freq < 6.2e9 )
	{
		ifreq = lrnd( freq * 1.0e-3 );
		if ( ifreq == 0 )
			ifreq = 1;
	}
	else if ( freq < 12.4e9 )
		ifreq = 2 * lrnd( freq / 2.0e3 );
	else
		ifreq = 3 * lrnd( freq / 3.0e-3 );

	tfreq = ifreq;

	cmd[ 0 ] = 'W';
	while ( tfreq >= 10 )
	{
		cmd[ 0 ]--;
		tfreq /= 10;
	}

	sprintf( cmd + 1, "%ldZ8\n", ifreq );
	if ( gpib_write( hp8672a.device, cmd, strlen( cmd ) ) == FAILURE )
		hp8672a_comm_failure( );

	/* Frequency needs about 10 ms to settle */

	fsc2_usleep( 10000, UNSET );

	return ifreq * 1.0e3;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8672a_set_attenuation( double att )
{
	char cmd[ 100 ];
	int a;


	fsc2_assert( att >= MAX_ATTEN && att <= hp8672a.min_attenuation );

	if ( att > 3 )
	{
		hp8672a.is_10db = SET;
		att -= 10;
	}

	a = irnd( - att ) + 3;

	if ( a < 100 )
		sprintf( cmd, "K%02d\n", a );
	else if ( a < 110 )
		sprintf( cmd, "K:%1d\n", a - 100 );
	else if ( a < 120 )
		sprintf( cmd, "K;%1d\n", a - 110 );
	else
		sprintf( cmd, "K;%c\n", ':' + ( a - 120 ) );
		
	if ( gpib_write( hp8672a.device, cmd, strlen( cmd ) ) == FAILURE )
		hp8672a_comm_failure( );

	/* Switch between extra 10 db on and off if necessary */

	if ( hp8672a.state == SET )
		hp8672a_output_state( SET );

	return att;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

int hp8672a_set_modulation( void )
{
	char cmd[ 10 ];


	fsc2_assert( hp8672a.mod_type_is_set );

	if ( hp8672a.mod_type == MOD_TYPE_OFF )
	{
		if ( gpib_write( hp8672a.device, "M06\n", 4 ) == FAILURE )
			hp8672a_comm_failure( );
		return 1;
	}

	if ( hp8672a.mod_type == MOD_TYPE_AM )
	{
		sprintf( cmd, "M%d6\n", 2 - hp8672a.mod_ampl[ hp8672a.mod_type ] );
		if ( gpib_write( hp8672a.device, cmd, 4 ) == FAILURE )
			hp8672a_comm_failure( );

		fsc2_usleep( 15000, UNSET );

		return 1;
	}		

	sprintf( cmd, "M0%d\n", 5 - hp8672a.mod_ampl[ hp8672a.mod_type ] );
	if ( gpib_write( hp8672a.device, cmd, 4 ) == FAILURE )
		hp8672a_comm_failure( );

	fsc2_usleep( 50000, UNSET );

	return 1;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static void hp8672a_comm_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
