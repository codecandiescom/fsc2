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


#include "rs_smb100a.h"


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
inp_init( void )
{
	if ( FSC2_MODE == PREPARATION )
	{
		rs->inp.slope_has_been_set = false;
		rs->inp.imp_has_been_set   = false;

		return;
	}

	if ( FSC2_MODE == TEST )
	{
		if ( rs->inp.slope_has_been_set )
		{
			rs->inp.slope = SLOPE_POSITIVE;
			rs->inp.slope_has_been_set = true;
		}

		if ( ! rs->inp.imp_has_been_set )
		{
			rs->inp.imp = IMPEDANCE_HIGH;
			rs->inp.imp_has_been_set = true;
		}

		return;
	}

	if ( ! ( rs->inp.slope_has_been_set ^= 1 ) )
		inp_set_slope( rs->inp.slope );
	else
		rs->inp.slope = query_slope( "INP:TRIG:SLOPE?" );

	if ( ! ( rs->inp.imp_has_been_set ^= 1 ) )
		inp_set_impedance( rs->inp.imp );
	else
	{
		rs->inp.imp = query_imp( "INP:MOD:IMP?" );

		if (    rs->inp.imp != IMPEDANCE_G600
			 && rs->inp.imp != IMPEDANCE_HIGH )
			bad_data( );
	}
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Slope
inp_slope( void )
{
	if ( ! rs->inp.slope_has_been_set )
	{
		print( FATAL, "Trigger input slope has not been set yet.\n" );
		THROW( EXCEPTION );
	}

	return rs->inp.slope;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Slope
inp_set_slope( enum Slope slope )
{
    if ( rs->inp.slope_has_been_set && slope == rs->inp.slope )
        return rs->inp.slope;

	if ( slope != SLOPE_POSITIVE && slope != SLOPE_NEGATIVE )
	{
		print( FATAL, "Invalid trigger slope %d, use either \"POSITIVE\" or "
			   "\"NEGATIVE\".\n", slope );
		THROW( EXCEPTION );
	}

	rs->inp.slope_has_been_set = true;

	if ( FSC2_MODE != EXPERIMENT )
		return rs->inp.slope - slope;

    char cmd[ 19 ] = "INP:TRIG:SLOPE ";
    strcat( cmd, slope == SLOPE_POSITIVE ? "POS" : "NEG" );
    rs_write( cmd );

    return rs->inp.slope = slope;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
inp_impedance( void )
{
	if ( ! rs->inp.imp_has_been_set )
	{
		print( FATAL, "Modulation input impedance has not been set yet.\n" );
		THROW( EXCEPTION );
	}

	return rs->inp.imp;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
inp_set_impedance( enum Impedance imp )
{
    if ( rs->inp.imp_has_been_set && imp == rs->inp.imp )
        return rs->inp.imp;

    if ( imp != IMPEDANCE_G600 && imp != IMPEDANCE_HIGH )
	{
		print( FATAL, "Invalid modulation input impedance %d requested, "
			   "use either \"G600\" or \"HIGH\".\n" );
		THROW( EXCEPTION );
	}

	rs->inp.imp_has_been_set = true;

	if ( FSC2_MODE != EXPERIMENT )
		return rs->inp.imp = imp;

    char cmd[ 17 ] = "INP:MOD:IMP ";
    strcat( cmd, imp == IMPEDANCE_G600 ? "G600" : "HIGH" );

	rs_write( cmd );
    return rs->inp.imp = imp;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
