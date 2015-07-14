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


static void pulm_prep_init( void );
static void pulm_test_init( void );
static void pulm_exp_init( void );
static void check_has_pulse_mod( void );


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
pulm_init( void )
{
    if ( ! rs->pulm.has_pulse_mod )
        return;

    if ( FSC2_MODE == PREPARATION )
        pulm_prep_init( );
    else if ( FSC2_MODE == TEST )
        pulm_test_init( );
    else
        pulm_exp_init( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
pulm_prep_init( void )
{
#if defined B21 || defined B22    // pulse modulation option present?
    rs->pulm.has_pulse_mod = true;
#else
    rs->pulm.has_pulse_mod = false;
#endif

    rs->pulm.state = false;

    rs->pulm.pol_has_been_set = false;
    rs->pulm.imp_has_been_set = false;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
pulm_test_init( void )
{
    if ( rs->pulm.pol_has_been_set )
    {
        rs->pulm.pol = POLARITY_NORMAL;
        rs->pulm.pol_has_been_set = true;
    }

    if ( rs->pulm.imp_has_been_set )
    {
        rs->pulm.imp = IMPEDANCE_G50;
        rs->pulm.imp_has_been_set = true;
    }
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
pulm_exp_init( void )
{
    rs_write( "PULM:SOUR EXT" );

    rs->pulm.state = query_bool( "PULM:STAT?" );

    if ( ! ( rs->pulm.pol_has_been_set ^= 1 ) )
        pulm_set_polarity( rs->pulm.pol );
    else
        rs->pulm.pol = query_pol( "PULM:POL?" );

    if ( ! ( rs->pulm.imp_has_been_set ^= 1 ) )
        pulm_set_impedance( rs->pulm.imp );
    else
    {
        rs->pulm.imp  = query_imp( "PULM:TRIG:EXT:IMP?" );

        if (    rs->pulm.imp != IMPEDANCE_G50
             && rs->pulm.imp != IMPEDANCE_G10K )
            bad_data( );
    }
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
pulm_state( void )
{
    if ( ! rs->pulm.has_pulse_mod )
        return false;

    return rs->pulm.state;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
pulm_set_state( bool state )
{
    check_has_pulse_mod( );

    if ( state == rs->pulm.state )
        return rs->pulm.state;

    if ( FSC2_MODE != EXPERIMENT )
        return rs->pulm.state = state;

    char cmd[ ] = "PULM:STAT *";
    cmd[ 10 ] = state ? '1' : '0';
    rs_write( cmd );

    return rs->pulm.state = state;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Polarity
pulm_polarity( void )
{
    check_has_pulse_mod( );

    if ( ! rs->pulm.pol_has_been_set )
    {
        print( FATAL, "Pulse modulation polarity hasn't been set yet.\n" );
        THROW( EXCEPTION );
    }

    return rs->pulm.pol;
}


/*----------------------------------------------------*
 * Set polarity: for Normal polarity RF is on while external
 * pulse is present, for Inverted RF is off while pulse is
 * present.
 *----------------------------------------------------*/

enum Polarity
pulm_set_polarity( enum Polarity pol )
{
    check_has_pulse_mod( );

    if ( rs->pulm.pol_has_been_set && pol == rs->pulm.pol )
        return rs->pulm.pol;

    if ( pol != POLARITY_NORMAL && pol != POLARITY_INVERTED )
    {
        print( FATAL, "Invalid polarity value %d.\n", pol );
        THROW( EXCEPTION );
    }

    rs->pulm.pol_has_been_set = true;

    if ( FSC2_MODE != EXPERIMENT )
        return rs->pulm.pol = pol;

    char cmd[ 13 ] = "PULM:POL ";
    strcat( cmd, pol == POLARITY_NORMAL ? "NORM" : "INV" );
    rs_write( cmd );

    return rs->pulm.pol = pol;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
pulm_impedance( void )
{
    check_has_pulse_mod( );

    if ( ! rs->pulm.pol_has_been_set )
    {
        print( FATAL, "Pulse modulation input impedance hasn't been set "
               "yet.\n" );
        THROW( EXCEPTION );
    }

    return rs->pulm.imp;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
pulm_set_impedance( enum Impedance imp )
{
    check_has_pulse_mod( );

    if ( rs->pulm.imp_has_been_set && imp == rs->pulm.imp )
        return rs->pulm.imp;

    if ( imp != IMPEDANCE_G50 && imp != IMPEDANCE_G10K )
    {
        print( FATAL, "Invalid pulse modulatiuon input impedance %d requested, "
               "use either \"G50\" or \"G10K\".\n", imp );
        THROW( EXCEPTION );
    }

    rs->pulm.imp_has_been_set = true;

    if ( FSC2_MODE != EXPERIMENT )
        return rs->pulm.imp = imp;

    char cmd[ 23 ] = "PULM:TRIG:EXT:IMP ";
    strcat( cmd, imp == IMPEDANCE_G50 ? "G50" : "G10K" );
    rs_write( cmd );

    return rs->pulm.imp = imp;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
check_has_pulse_mod( void )
{
    if ( ! rs->pulm.has_pulse_mod )
    {
        print( FATAL, "Function can't be used, module was not compiled "
               "with support for pulse modulation.\n" );
        THROW( EXCEPTION );
    }
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
pulm_available( void )
{
    return rs->pulm.has_pulse_mod;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
