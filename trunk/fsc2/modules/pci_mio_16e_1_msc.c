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


#include "pci_mio_16e_1.h"


/*---------------------------------------------------------------------*/
/* Functions allows to reserve (or un-reserve) the FREQ_OUT so that in */
/* the following changes to the FREQ_OUT require a pass-phrase (i.e.   */
/* when calling the function daq_freq_out() withb an argument.         */
/*---------------------------------------------------------------------*/

Var *daq_reserve_freq_out( Var *v )
{
	bool lock_state = SET;


	if ( v == NULL )
		return vars_push( INT_VAR,
						  pci_mio_16e_1.msc_state.reserved_by ? 1L : 0L );

	if ( v->type != STR_VAR )
	{
		print( FATAL, "Argument isn't a pass-phrase.\n" );
		THROW( EXCEPTION );
	}

	if ( v->next != NULL )
	{
		lock_state = get_boolean( v->next );
		too_many_arguments( v->next );
	}

	if ( pci_mio_16e_1.msc_state.reserved_by )
	{
		if ( lock_state == SET )
		{
			if ( ! strcmp( pci_mio_16e_1.msc_state.reserved_by, v->val.sptr ) )
				return vars_push( INT_VAR, 1L );
			else
				return vars_push( INT_VAR, 0L );
		}
		else
		{
			if ( ! strcmp( pci_mio_16e_1.msc_state.reserved_by, v->val.sptr ) )
			{
				pci_mio_16e_1.msc_state.reserved_by =
						  CHAR_P T_free( pci_mio_16e_1.msc_state.reserved_by );
				return vars_push( INT_VAR, 1L );
			}
			else
				return vars_push( INT_VAR, 0L );
		}
	}

	if ( ! lock_state )
		return vars_push( INT_VAR, 1L );

	pci_mio_16e_1.msc_state.reserved_by = T_strdup( v->val.sptr );
	return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------------------*/
/* Function for setting the output at the FREQ_OUT pin. It can be queried, */
/* returning the currently output frequency or 0 if switched off, or can   */
/* set to the frequency passed to the function (within the limits of the   */
/* range of possible frequencies) or switched off when the requested       */
/* frequency is 0.                                                         */
/*-------------------------------------------------------------------------*/

Var *daq_freq_out( Var *v )
{
	double freq;
	double new_freq;
	NI_DAQ_CLOCK_TYPE daq_clock;
	NI_DAQ_STATE on_off;
	NI_DAQ_CLOCK_SPEED_VALUE speed;
	int divider;
	double freqs[ 4 ] = { 2.0e7, 1.0e7, 2.0e5, 1.0e5 };
	int indx[ 4 ] = { 0, 0, 0, 0 };
	int i, j;
	char *pass;


	/* In the real experiment ask the driver for the current clock settings,
	   otherwise take some dummy values */

	if ( FSC2_MODE == EXPERIMENT )
		ni_daq_msc_get_clock_state( pci_mio_16e_1.board, &daq_clock, &on_off,
									&speed, &divider );
	else
	{
		daq_clock = pci_mio_16e_1.msc_state.daq_clock;
		on_off = pci_mio_16e_1.msc_state.on_off;
		speed = pci_mio_16e_1.msc_state.speed;
		divider = pci_mio_16e_1.msc_state.divider;
	}

	/* Lets see which clock is currently in use */

	if ( daq_clock == NI_DAQ_FAST_CLOCK )
		freq = 2.0e7;
	else
		freq = 2.0e5;

	/* Is it running at half the maximum speed ? */

	if ( speed == NI_DAQ_HALF_SPEED )
		freq *= 0.5;

	/* If the user asked for the frequency tell him, taking into account the
	   current divider. If output is switched off tell user it's 0 Hz. */

	if ( v == NULL )
	{
		if ( on_off == NI_DAQ_DISABLED )
			freq = 0.0;
		else
			freq /= divider;

		return vars_push( FLOAT_VAR, freq );
	}

	if ( v != NULL && v->type == STR_VAR )
	{
		pass = T_strdup( v->val.sptr );

		if ( pci_mio_16e_1.msc_state.reserved_by )
		{
			if ( strcmp( pci_mio_16e_1.msc_state.reserved_by, pass ) )
			{
				print( FATAL, "FRQ_OUT is reserved, wrong phase-phrase.\n" );
				T_free( pass );
				THROW( EXCEPTION );
			}
		}
		else 
			print( WARN, "Pass-phrase for non-reserved FREQ_OUT.\n" );

		T_free( pass );
		v = vars_pop( v );
	}
	else if ( pci_mio_16e_1.msc_state.reserved_by )
	{
		print( FATAL, "FREQ_OUT is reserved, phase-phrase required.\n" );
		THROW( EXCEPTION );
	}

	/* Get the frequency the user asked for */

	new_freq = get_double( v, "output clock speed" );

	if ( new_freq < 0.0 )
	{
		print( FATAL, "Invalid negative output frequency.\n" );
		THROW( EXCEPTION );
	}

	/* If it's basically zero switch off the output */

	if ( new_freq <= 1.0e-9 )
	{
		if ( ni_daq_msc_set_clock_output( pci_mio_16e_1.board, daq_clock,
										  NI_DAQ_DISABLED ) < 0 )
		{
			print( FATAL, "Failed to switch off frequency output.\n" );
			THROW( EXCEPTION );
		}

		return vars_push( FLOAT_VAR, 0.0 );
	}

	/* Now comes the tricky part - we need to figure out if it's possible
	   to output the requested frequency (or a frequency not deviating too
	   much from what the user wants) and how to do it. We have four
	   frequencies we can use - the full and the half frequency of both the
	   fast and the slow clock. All of these can be divided by a number
	   between 1 and 16. Now we test with which comnbinations we can get
	   a near to the requested frequency. */

	for ( i = 0; i < 4; i++ )
		for ( j = 1; j < 16; j++ )
			if ( new_freq <= freqs[ i ] / j &&
				 new_freq > freqs[ i ] / ( j + 1 ) )
			{
				if ( ( new_freq * j ) / freqs[ i ] >
					 				freqs[ i + 1 ] / ( ( j + 1 ) * new_freq ) )
					indx[ i ] = j;
				else
					indx[ i ] = j + 1;
			}

	if ( new_freq > freqs[ 0 ] )
		indx[ 0 ] = 1;
	else if ( new_freq < freqs[ 3 ] / 16 )
		indx[ 3 ] = 16;

	/* Since the ranges that can be produced with the fast and the slow
	   clock don't overlap we can concentrate on one of the alternatives. */

	if ( indx[ 0 ] == 0 )
	{
		freq = 2.0e5;
		daq_clock = NI_DAQ_SLOW_CLOCK;
		indx[ 0 ] = indx[ 2 ];
		indx[ 1 ] = indx[ 3 ];
	}
	else
	{
		freq = 2.0e7;
		daq_clock = NI_DAQ_FAST_CLOCK;
	}

	/* If the frequency can be only created with either the full speed or
	   with half the full speed we already got a winner, otherwise we
	   still have to figure out which one is better suited */

	speed = NI_DAQ_FULL_SPEED;
	divider = indx[ 0 ];

	if ( ( indx[ 0 ] == 0 && indx[ 1 ] != 0 ) ||
		 ( indx[ 0 ] != 0 && indx[ 1 ] != 0 &&
		   fabs( freq / indx[ 0 ] - new_freq ) >
		   fabs( 0.5 * freq / indx[ 1 ] - new_freq ) ) )
	{
		speed = NI_DAQ_HALF_SPEED;
		divider = indx[ 1 ];
		freq *= 0.5 / divider;
	}
	else
		freq /= divider;

	/* Warn teh user about deviations of more than a promille from the
	   requested frequency */

	if ( fabs( freq - new_freq ) > 0.001 * new_freq )
		print( SEVERE, "Can't output requested frequency of %.3f kHz, using "
			   "%.3f kHz instead.\n", new_freq * 1.0e-3, freq * 1.0e-3 );

	/* Set clock, frequency and switch on output during the experiment,
	   otherwise just store the settings in case their going to be
	   requested some time later. */

	if ( FSC2_MODE == EXPERIMENT )
	{
		if ( ni_daq_msc_set_clock_speed( pci_mio_16e_1.board, speed,
										 divider ) < 0 ||
			 ni_daq_msc_set_clock_output( pci_mio_16e_1.board, daq_clock,
										  on_off ) < 0 )
		{
			print( FATAL, "Failed to set frequency output.\n ");
			THROW( EXCEPTION );
		}
	}
	else
	{
		pci_mio_16e_1.msc_state.daq_clock = daq_clock;
		pci_mio_16e_1.msc_state.on_off = on_off;
		pci_mio_16e_1.msc_state.speed = speed;
		pci_mio_16e_1.msc_state.divider = divider;
	}

	return vars_push( FLOAT_VAR, freq );
}


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

Var *daq_trigger_setup( Var *v )
{
	NI_DAQ_TRIG_TYPE trigger_type;
	const char *tt[ ] = { "TTL", "Low_Window", "High_Window", "Middle_Window",
						  "High_Hysteresis", "Low_Hysteresis" };
	double level;
	double tl = -10.0;
	double th = 10.0;
	size_t i;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	if ( v->type != STR_VAR )
	{
		print( FATAL, "Missing trigger type argument.\n" );
		THROW( EXCEPTION );
	}

	/* Determine the trigger type */

	for ( i = 0; i < sizeof tt / sizeof tt[ 0 ]; i++ )
		if ( ! strcasecmp( v->val.sptr, tt[ i ] ) )
			break;

	switch ( i )
	{
		case 0 :
			trigger_type = NI_DAQ_TRIG_TTL;
			break;

		case 1 :
			trigger_type = NI_DAQ_TRIG_LOW_WINDOW;
			break;

		case 2 :
			trigger_type = NI_DAQ_TRIG_HIGH_WINDOW;
			break;

		case 3 :
			trigger_type = NI_DAQ_TRIG_MIDDLE_WINDOW;
			break;
			
		case 4 :
			trigger_type = NI_DAQ_TRIG_HIGH_HYSTERESIS;
			break;

		case 5 :
			trigger_type = NI_DAQ_TRIG_LOW_HYSTERESIS;
			break;

		default :
			print( FATAL, "Invalid trigger type argument.\n" );
			THROW( EXCEPTION );
	}

	/* All trigger types except "Digital" require at least one triger level
	   argument */

	if ( trigger_type != NI_DAQ_TRIG_TTL )
	{
		if ( ( v = vars_pop( v ) ) == NULL )
		{
			print( FATAL, "Missing trigger level argument.\n" );
			THROW( EXCEPTION );
		}

		level = get_double( v, "trigger level" );

		/* "Low_Window" and "High_Window" mode require a single trigger level
		   (the other one gets automatically set to the maximum value), all
		   others need also a second trigger (the low level) argument */

		if ( trigger_type == NI_DAQ_TRIG_LOW_WINDOW )
		{
			if ( level < -10.001 )
			{
				print( FATAL, "Low trigger level is below -10 V.\n" );
				THROW( EXCEPTION );
			}

			tl = level;
		}
		else if ( trigger_type == NI_DAQ_TRIG_HIGH_WINDOW )
		{
			if ( level > 10.001 )
			{
				print( FATAL, "High trigger level is above 10 V.\n" );
				THROW( EXCEPTION );
			}

			th = level;
		}
		else
		{
			if ( level >= 10.001 )
			{
				print( FATAL, "High trigger level is above 10 V.\n" );
				THROW( EXCEPTION );
			}

			th = level;

			if ( ( v = vars_pop( v ) ) == NULL )
			{
				print( FATAL, "Missing second trigger level argument.\n" );
				THROW( EXCEPTION );
			}

			tl = get_double( v, "trigger level" );

			if ( tl <= -10.001 )
			{
				print( FATAL, "Low trigger level is below -10 V.\n" );
				THROW( EXCEPTION );
			}
		}

		if ( tl >= th )
		{
			print( FATAL, "Low trigger level isn't below upper level.\n" );
			THROW( EXCEPTION );
		}
	}

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT &&
		 ni_daq_msc_set_trigger( pci_mio_16e_1.board,
								 trigger_type, th, tl ) < 0 )
		{
			print( FATAL, "Failed to set trigger mode (and level).\n" );
			THROW( EXCEPTION );
		}

	return vars_push( INT_VAR, 1L );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
