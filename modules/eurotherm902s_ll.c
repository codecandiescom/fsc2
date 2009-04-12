/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


#include "bvt3000.h"


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

bool
eurotherm902s_check_sensor_break( void )
{
    return eurotherm902s_get_sw( ) & 0x02 ? SET : UNSET;

}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
eurotherm902s_get_temperature( void )
{
    return T_atod( bvt3000_query( "PV" ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
eurotherm902s_set_mode( int mode )
{
    unsigned sw = eurotherm902s_get_sw( );


    if (    ( mode == AUTOMATIC_MODE && sw & 0x80 )
         || ( mode == MANUAL_MODE && ! ( sw & 0x80 ) ) )
        return;

    if ( mode == AUTOMATIC_MODE )
        sw &= ~ 0x80;
    else
        sw |= 0x80;

    eurotherm902s_set_sw( sw );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

int
eurotherm902s_get_mode( void )
{
    return eurotherm902s_get_sw( ) & 0x80 ? AUTOMATIC_MODE : MANUAL_MODE;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
eurotherm902s_set_active_setpoint( int sp )
{
    unsigned int sw = eurotherm902s_get_sw( );


    if ( ( sp == SP1 && ! ( sw & 0x20 ) ) || ( sp == SP2 && sw & 0x20 ) )
        return;

    if ( sp == SP1 )
        sw &= 0xDF;
    else
        sw |= 0x20;
    eurotherm902s_set_sw( sw );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

int
eurotherm902s_get_active_setpoint( void )
{
    return eurotherm902s_get_sw( ) & 0x20 ? SP2 : SP1;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
eurotherm902s_set_setpoint( double temp )
{
    char buf[ 20 ];


    sprintf( buf, "SL% 6.1f",  temp );
    bvt3000_send_command( buf );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
eurotherm902s_get_setpoint( void )
{
    return T_atod( bvt3000_query( "SL" ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
eurotherm902s_get_working_setpoint( void )
{
    return T_atod( bvt3000_query( "SP" ) );
}


/*-------------------------------*
 * Function sets the status word
 *-------------------------------*/

void
eurotherm902s_set_sw( unsigned int sw )
{
    char buf[ 10 ];


    fsc2_assert( sw <= 0xFFFF );

    sprintf( buf, "SW>%04x", sw & 0xE005 );
    bvt3000_send_command( buf );
}


/*---------------------------------------*
 * Function returns value of status word 
 *---------------------------------------*/

unsigned int
eurotherm902s_get_sw( void )
{
    unsigned int sw;
    char *reply = bvt3000_query( "SW" );


    if (    reply[ 0 ] != '>'
         || sscanf( reply + 1, "%x", &sw ) != 1
         || sw > 0xFFFF )
        bvt3000_comm_fail( );
    return sw;
}


/*----------------------------------------*
 * Function sets the optional status word
 *---------------------------------------*/

void
eurotherm902s_set_os( unsigned int os )
{
    char buf[ 10 ];


    fsc2_assert( os <= 0xFFFF );

    sprintf( buf, "OS>%04x", os & 0x20B3 );
    bvt3000_send_command( buf );
}


/*------------------------------------------------*
 * Function returns value of optional status word 
 *------------------------------------------------*/

unsigned int
eurotherm902s_get_os( void )
{
    unsigned int os;
    char *reply = bvt3000_query( "OS" );


    if (    reply[ 0 ] != '>'
         || sscanf( reply + 1, "%x", &os ) != 1
         || os > 0xFFFF )
        bvt3000_comm_fail( );

    return os;
}


/*----------------------------------------*
 * Function sets the extended status word
 *----------------------------------------*/

void
eurotherm902s_set_xs( unsigned int xs )
{
    char buf[ 10 ];


    fsc2_assert( xs <= 0xFFFF );

    sprintf( buf, "OS>%04x", xs & 0xFFB7 );
    bvt3000_send_command( buf );
}


/*------------------------------------------------*
 * Function returns value of extended status word 
 *------------------------------------------------*/

unsigned int
eurotherm902s_get_xs( void )
{
    unsigned int xs;
    char *reply = bvt3000_query( "XS" );


    if (    reply[ 0 ] != '>'
         || sscanf( reply + 1, "%x", &xs ) != 1
         || xs > 0xFFFF )
        bvt3000_comm_fail( );

    return xs;
}


/*------------------------------------------------------------*
 * Returns the minimum value that can be set for the setpoint
 *------------------------------------------------------------*/

double
eurotherm902s_get_min_setpoint( void )
{
    return T_atod( bvt3000_query( "LS" ) );
}


/*---------------------------------------------------------------*
 * Returns the maximum value that can be set for the setpoint
 *---------------------------------------------------------------*/

double
eurotherm902s_get_max_setpoint( void )
{
    return T_atod( bvt3000_query( "HS" ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
eurotherm902s_set_heater_power_limit( double power )
{
    char buf[ 20 ];


    sprintf( buf, "HO% 6.1f", power );
    bvt3000_send_command( buf );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
eurotherm902s_get_heater_power_limit( void )
{
    return T_atod( bvt3000_query( "HO" ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
eurotherm902s_set_heater_power( double power )
{
    char buf[ 20 ];


    sprintf( buf, "OP% 6.1f", power );
    bvt3000_send_command( buf );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
eurotherm902s_get_heater_power( void )
{
    return T_atod( bvt3000_query( "OP" ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
eurotherm902s_set_proportional_band( double pb )
{
    char buf[ 20 ];


    sprintf( buf, "XP% 6.1f", pb );
    bvt3000_send_command( buf );
}
    

/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
eurotherm902s_get_proportional_band( void )
{
    return T_atod( bvt3000_query( "XP" ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
eurotherm902s_set_integral_time( double t )
{
    char buf[ 20 ];


    sprintf( buf, "TI% 6.1f", t );
    bvt3000_send_command( buf );
}
    

/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
eurotherm902s_get_integral_time( void )
{
    return T_atod( bvt3000_query( "TI" ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
eurotherm902s_set_derivative_time( double t )
{
    char buf[ 20 ];


    sprintf( buf, "TD% 6.1f", t );
    bvt3000_send_command( buf );
}
    

/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
eurotherm902s_get_derivative_time( void )
{
    return T_atod( bvt3000_query( "TD" ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
eurotherm902s_set_cutback_high( double cb )
{
    char buf[ 20 ];


    sprintf( buf, "HB% 6.1f", cb );
    bvt3000_send_command( buf );
}
    

/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
eurotherm902s_get_cutback_high( void )
{
    return T_atod( bvt3000_query( "HB" ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
eurotherm902s_set_cutback_low( double cb )
{
    char buf[ 20 ];


    sprintf( buf, "LB% 6.1f", cb );
    bvt3000_send_command( buf );
}
    

/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
eurotherm902s_get_cutback_low( void )
{
    return T_atod( bvt3000_query( "LB" ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
eurotherm902s_lock_keyboard( bool lock )
{
    unsigned int sw = eurotherm902s_get_sw( );


    if ( ( lock && sw & 0x04 ) || ( ! lock && ! ( sw & 0x04 ) ) )
        return;

    if ( lock )
        sw |= 0x04;
    else
        sw &= ~ 0x04;

    eurotherm902s_set_sw( sw );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
