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
outp_init( void )
{
    if ( FSC2_MODE == PREPARATION )
    {
        rs->outp.state_has_been_set = false;
        return;
    }

    if ( FSC2_MODE == TEST )
    {
        if ( ! rs->outp.state_has_been_set )
        {
            rs->outp.state = false;
            rs->outp.state_has_been_set = true;
        }

        rs->outp.imp = IMPEDANCE_G50;
        return;
    }

    /* Switch to auto-attenuator mode to be able to use the full attenuator
       range */

    rs_write( "OUTP:AMOD AUTO" );

    /* Switch RF power off (or to minimum level) during ALC searches */

    rs_write( "OUTP:ALC:SEAR:MODE MIN" );

    if ( ! ( rs->outp.state_has_been_set ^= 1 ) )
        outp_set_state( rs->outp.state );
    else
        rs->outp.state = query_bool( "OUTP:STATE?" );

    rs->outp.imp   = query_imp( "OUTP:IMP?" );
    if (    rs->outp.imp != IMPEDANCE_G50
         && rs->outp.imp != IMPEDANCE_G1K
         && rs->outp.imp != IMPEDANCE_G10K )  
        bad_data( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
outp_state( void )
{
    if ( ! rs->outp.state_has_been_set )
    {
        print( FATAL, "RF output state hasn't been set yet.\n" );
        THROW( EXCEPTION );
    }

    return rs->outp.state;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
outp_set_state( bool state )
{
    if ( rs->outp.state_has_been_set && state == rs->outp.state )
        return rs->outp.state;

    // Check if we're currently processing a list and, if yes, stop it.

    if ( ! state )
        list_stop( false );

    rs->outp.state_has_been_set = true;

    // Switching output off stops list processing mode!

    if ( ! state )
        rs->list.processing_list = false;

    if ( FSC2_MODE != EXPERIMENT )
        return rs->outp.state = state;

    char cmd[ ] = "OUTP:STATE x";
    cmd[ 11 ] = state ? '1' : '0';
    rs_write( cmd );
\
    return rs->outp.state = state;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
outp_impedance( void )
{
    return rs->outp.imp;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
outp_protection_is_tripped( void )
{
    if ( FSC2_MODE != EXPERIMENT || ! rs->has_reverse_power_protection )
        return false;

    return query_bool( "OUTP:PROT:TRIP?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
outp_reset_protection( void )
{
    if ( FSC2_MODE != EXPERIMENT || ! rs->has_reverse_power_protection )
        return rs->outp.state;

    rs_write( "OUTP:PROT:CLE" );
    return rs->outp.state = query_bool( "OUTP:STATE?" );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
