/* -*-C-*-
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


#include "rs.h"


static char const * mod_names[ ] = { "amplitude", "frequency",
									 "phase",     "pulse" };

static Mod_Funcs mod_funcs[ ] = { { am_state,       am_set_state,
									am_depth,       am_set_depth,
									am_coupling,    am_set_coupling,
									am_source,      am_set_source,
									NULL,           NULL,
									NULL,           NULL,
									NULL,           NULL },
								  { fm_state,       fm_set_state,
									fm_deviation,   fm_set_deviation,
									fm_coupling,    fm_set_coupling,
									fm_source,      fm_set_source,
									fm_mode,        fm_set_mode,
									NULL, NULL,	    
									NULL, NULL },   
								  { pm_state,       pm_set_state,
									pm_deviation,   pm_set_deviation,
									pm_coupling,    pm_set_coupling,
									pm_source,      pm_set_source,
									pm_mode,        pm_set_mode,
									NULL,           NULL,
									NULL,           NULL },
#if defined WITH_PULSE_MODULATION
								  { pulm_state,     pulm_set_state,
									NULL,           NULL,
									NULL,           NULL,
									NULL,           NULL,
									NULL,           NULL,
									pulm_impedance, pulm_set_impedance,
									pulm_polarity,  pulm_set_polarity } };
#else
								  { pulm_state,     NULL,
									NULL,           NULL,
									NULL,           NULL,
									NULL,           NULL,
									NULL,           NULL,
									NULL,           NULL,
									NULL,           NULL } };
#endif


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
mod_init( void )
{
	rs->mod.funcs = mod_funcs;

	if ( FSC2_MODE == PREPARATION )
	{
		rs->mod.type_has_been_set = false;
		for ( enum Mod_Type i = MOD_TYPE_AM; i <= MOD_TYPE_PULM; i++ )
			rs->mod.state_has_been_set[ i ] = false;
		return;
	}

	if ( FSC2_MODE == TEST )
		return;

	if ( ! rs->mod.type_has_been_set )
	{
		/* If no modulation type had been set check if just one modulation
		   sub-system is on leave as it is, otherwise switch all off. */

		int cnt = 0;
		for ( enum Mod_Type i = MOD_TYPE_AM; i <= MOD_TYPE_PULM; i++ )
		{
			if ( ( rs->mod.state[ i ] = rs->mod.funcs[ i ].get_state( ) ) )
				cnt++;

			rs->mod.state_has_been_set[ i ] = true;
		}

		if ( cnt > 1 )
		{
			for ( enum Mod_Type i = MOD_TYPE_AM; i <= MOD_TYPE_PULM; i++ )
				if ( rs->mod.state[ i ] )
					rs->mod.funcs[ i ].set_state( false );
		}
	}
	else
	{
		enum Mod_Type sel_type = rs->mod.type;

		/* For all modulation sub-systems that have no state set figure it
		   out from the current setting. Switch off all except the
		   selected type */

		for ( enum Mod_Type i = MOD_TYPE_AM; i <= MOD_TYPE_PULM; i++ )
		{
			if ( ! rs->mod.state_has_been_set[ i ] )
			{
				rs->mod.state[ i ] = rs->mod.funcs[ i ].get_state( );
				rs->mod.state_has_been_set[ i ] = true;
			}

			if ( i != sel_type && rs->mod.funcs[ i ].set_state )
				rs->mod.funcs[ i ].set_state( false );
		}

		// Make sure the selected system is in the correct state

		rs->mod.funcs[ sel_type ].set_state( rs->mod.state[ sel_type ] );
	}
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Mod_Type
mod_type( void )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set.\n" );
		THROW( EXCEPTION );
	}

	return rs->mod.type;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Mod_Type
mod_set_type( enum Mod_Type type )
{
	if ( rs->mod.type_has_been_set && type == rs->mod.type )
		return rs->mod.type;

	// Check the argument

	bool found = false;
	for ( enum Mod_Type i = MOD_TYPE_AM; i <= MOD_TYPE_PULM; i++ )
		if ( i == type )
		{
			found = true;
			break;
		}

	if ( ! found )
	{
		print( FATAL, "Invalid modulation type requested.\n" );
		THROW( EXCEPTION );
	}

	// Don't allow PULM if not available

	if ( type == MOD_TYPE_PULM && ! pulm_available( ) )
	{
		print( FATAL, "Pulse modulation can't be used, module was not compiled "
			   "with support for pulse modulation.\n" );
		THROW( EXCEPTION );
	}

	// Switch of a previously selected modulation system

	if ( rs->mod.type_has_been_set && rs->mod.funcs[ rs->mod.type ].set_state )
		rs->mod.funcs[ rs->mod.type ].set_state( false );

	// If a state has been set for the new modulation system switch it of

	if ( rs->mod.state_has_been_set[ type ] )
		rs->mod.funcs[ type ].set_state( rs->mod.state[ type ] );

	rs->mod.type_has_been_set = true;
	return rs->mod.type = type;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
mod_state( void )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set yet.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs->mod.state_has_been_set[ rs->mod.type ] )
	{
		print( FATAL, "Modulation state hasn't been set for %s modulation "
			   "yet.\n", mod_names[ rs->mod.type ] );
		THROW( EXCEPTION );
	}
		
	return rs->mod.state[ rs->mod.type ];
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
mod_set_state( bool state )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set yet.\n" );
		THROW( EXCEPTION );
	}

	rs->mod.state_has_been_set[ rs->mod.type ] = true;
	return rs->mod.state[ rs->mod.type ] =
		                       rs->mod.funcs[ rs->mod.type ].set_state( state );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
mod_amplitude( void )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set yet.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs->mod.funcs[ rs->mod.type ].get_amplitude )
	{
		print( FATAL, "Can't determine amplitude for %s modulation.\n",
			   mod_names[ rs->mod.type ] );
		THROW( EXCEPTION );
	}

	return rs->mod.funcs[ rs->mod.type ].get_amplitude( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
mod_set_amplitude( double amp )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set yet.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs->mod.funcs[ rs->mod.type ].set_amplitude )
	{
		print( FATAL, "Can't set amplitude for %s modulation.\n",
			   mod_names[ rs->mod.type ] );
		THROW( EXCEPTION );
	}

	return rs->mod.funcs[ rs->mod.type ].set_amplitude( amp );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Coupling
mod_coupling( void )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set yet.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs->mod.funcs[ rs->mod.type ].get_coupling )
	{
		print( FATAL, "Can't determine coupling for %s modulation.\n",
			   mod_names[ rs->mod.type ] );
		THROW( EXCEPTION );
	}

	return rs->mod.funcs[ rs->mod.type ].get_coupling( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Coupling
mod_set_coupling( enum Coupling coup )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set yet.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs->mod.funcs[ rs->mod.type ].set_coupling )
	{
		print( FATAL, "Can't set coupling for %s modulation.\n",
			   mod_names[ rs->mod.type ] );
		THROW( EXCEPTION );
	}

	return rs->mod.funcs[ rs->mod.type ].set_coupling( coup );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Source
mod_source( void )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set yet.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs->mod.funcs[ rs->mod.type ].get_source )
	{
		print( FATAL, "Can't determine source for %s modulation.\n",
			   mod_names[ rs->mod.type ] );
		THROW( EXCEPTION );
	}

	return rs->mod.funcs[ rs->mod.type ].get_source( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Source
mod_set_source( enum Source source )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set yet.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs->mod.funcs[ rs->mod.type ].set_source )
	{
		print( FATAL, "Can't set source for %s modulation.\n",
			   mod_names[ rs->mod.type ] );
		THROW( EXCEPTION );
	}

	return rs->mod.funcs[ rs->mod.type ].set_source( source );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Mod_Mode
mod_mode( void )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set yet.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs->mod.funcs[ rs->mod.type ].get_mode )
	{
		print( FATAL, "Can't determine mode for %s modulation.\n",
			   mod_names[ rs->mod.type ] );
		THROW( EXCEPTION );
	}

	return rs->mod.funcs[ rs->mod.type ].get_mode( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Mod_Mode
mod_set_mode( enum Mod_Mode mode )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set yet.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs->mod.funcs[ rs->mod.type ].set_mode )
	{
		print( FATAL, "Can't set mode for %s modulation.\n",
			   mod_names[ rs->mod.type ] );
		THROW( EXCEPTION );
	}

	return rs->mod.funcs[ rs->mod.type ].set_mode( mode );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
mod_impedance( void )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set yet.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs->mod.funcs[ rs->mod.type ].get_impedance )
	{
		print( FATAL, "Can't determine input impedance for %s modulation.\n",
			   mod_names[ rs->mod.type ] );
		THROW( EXCEPTION );
	}

	return rs->mod.funcs[ rs->mod.type ].get_impedance( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
mod_set_impedance( enum Impedance imp )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set yet.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs->mod.funcs[ rs->mod.type ].set_impedance )
	{
		print( FATAL, "Can't set input impedance for %s modulation.\n",
			   mod_names[ rs->mod.type ] );
		THROW( EXCEPTION );
	}

	return rs->mod.funcs[ rs->mod.type ].set_impedance( imp );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Polarity
mod_polarity( void )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set yet.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs->mod.funcs[ rs->mod.type ].get_polarity )
	{
		print( FATAL, "Can't polarity for %s modulation.\n",
			   mod_names[ rs->mod.type ] );
		THROW( EXCEPTION );
	}

	return rs->mod.funcs[ rs->mod.type ].get_polarity( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Polarity
mod_set_polarity( enum Polarity pol )
{
	if ( ! rs->mod.type_has_been_set )
	{
		print( FATAL, "No modulation type has been set yet.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs->mod.funcs[ rs->mod.type ].set_polarity )
	{
		print( FATAL, "Can't set polarity for %s modulation.\n",
			   mod_names[ rs->mod.type ] );
		THROW( EXCEPTION );
	}

	return rs->mod.funcs[ rs->mod.type ].set_polarity( pol );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
