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


#include "ep385.h"


static void ep385_gpib_failure( void );


#ifdef EP385_GPIB_DEBUG
#warning "**********************************"
#warning "ep385.so made for DEBUG mode only!"
#warning "**********************************"

#define gpib_write( a, b, c ) fprintf( stderr, "%s\n", ( b ) )
#define gpib_init_device( a, b ) 1
#endif


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

bool ep385_init( const char *name )
{
	char cmd[ 100 ];


#ifdef EP385_GPIB_DEBUG
	name = name;
#endif

	if ( gpib_init_device( name, &ep385.device ) == FAILURE )
		return FAIL;

	/* Stop and reset pulser */

	if ( gpib_write( ep385.device, "TIM;STQ\r", 8 ) == FAILURE )
		ep385_gpib_failure( );

	strcpy( cmd, "TIM;" );

	/* Set the trigger mode */

	if ( ep385.is_trig_in_mode && ep385.trig_in_mode == EXTERNAL )
		strcat( cmd, "STS1;" );
	else
		strcat( cmd, "STS3;IEC0;" );

	/* Set the shot-repetition time (if the user didn't specify one set the
	   shortest possible one) */

	if ( ep385.is_repeat_time )
		sprintf( cmd + strlen( cmd ), "SRT%ld;", ep385.repeat_time );
	else
		strcat( cmd, "SRT10;" );

	/* Set the time base source */

	if ( ep385.timebase_mode == EXTERNAL )
		strcat( cmd, "FRE0;" );
	else
		strcat( cmd, "FRE1;" );

	/* Set infinite repetition of the pulse sequence */

	strcat( cmd, "SPE65535;LPS1;SPL1;MEM16\r" );

	if ( gpib_write( ep385.device, cmd, strlen( cmd ) ) == FAILURE )
		ep385_gpib_failure( );

	/* Tell the pulser to accept the values and to enable the time base for
	   tiggering */

	if ( gpib_write( ep385.device, "TIM;SET;TRY\r", 12 ) == FAILURE )
		ep385_gpib_failure( );

	ep385_set_channels( );

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

bool ep385_run( bool state )
{
	const char *buf;


	buf = state ? "TIM;SET;TRY;SFT\r" : "TIM;STP\r";

	if ( gpib_write( ep385.device, buf , strlen( buf ) ) == FAILURE )
		ep385_gpib_failure( );

	ep385.is_running = state;

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

bool ep385_set_channels( void )
{
	int i, j, k;
	FUNCTION *f;
	CHANNEL *ch;
	char buf[ 1024 ];


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = ep385.function + i;

		if ( ! f->is_needed && f->num_channels == 0 )
			continue;

		for ( j = 0; j < f->num_channels; j++ )
		{
			ch = f->channel[ j ];

			if ( ! ch->needs_update )
				continue;

			sprintf( buf, "DIG;SLT%d;PSD2,",
					 f->channel[ j ]->self + CHANNEL_OFFSET );
			if ( ch->num_active_pulses == 0 )
				strcat( buf, "1,0,0,0,0\r" );
			else
			{
				sprintf( buf + strlen( buf ), "%d,", ch->num_active_pulses );
				for ( k = 0; k < ch->num_active_pulses; k++ )
					sprintf( buf + strlen( buf ), "%ld,0,%ld,0,",
							 ch->pulse_params[ k ].pos,
							 ch->pulse_params[ k ].pos +
							 ch->pulse_params[ k ].len );
				buf[ strlen( buf ) - 1 ] = '\r';
			}

			if ( gpib_write( ep385.device, buf, strlen( buf ) ) == FAILURE )
				ep385_gpib_failure( );
		}
	}

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static void ep385_gpib_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
