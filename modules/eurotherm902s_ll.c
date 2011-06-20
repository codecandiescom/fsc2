/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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


#include "bvt3000.h"


/*--------------------------------*
 * Initializes the Eurotherm 902S
 *--------------------------------*/

void
eurotherm902s_init( void )
{
    char *reply = bvt3000_query( "II" );
    unsigned int xs;


    if ( strcmp( reply, ">9020" ) )
    {
        print( FATAL, "Device doesn't have an Eurotherm 902S.\n" );
        THROW( EXCEPTION );
    }

    /* Make sure we're in normal operations mode */

    if ( eurotherm902s_get_operation_mode( ) != NORMAL_OPERATION )
        eurotherm902s_set_operation_mode( NORMAL_OPERATION );

    /* Check that there's no sensor break */

    if ( eurotherm902s_check_sensor_break( ) )
    {
        print( FATAL, "Temperature sensor not properly connected.\n" );
        THROW( EXCEPTION );
    }

    /* Always use SP1 */

    if ( eurotherm902s_get_active_setpoint( ) == SP2 )
    {
        eurotherm902s_set_setpoint( SP1, eurotherm902s_get_setpoint( SP2 ) );
        eurotherm902s_set_active_setpoint( SP1 );
    }    

    /* Only PID1 seems to be usable */

    if ( ( xs = eurotherm902s_get_xs( ) ) & ACTIVE_PID_FLAG )
        eurotherm902s_set_xs( xs & ~ ACTIVE_PID_FLAG );

    /* Enable SP + PID control (just a guess...) */

    if ( xs & PID_CONTROL_FLAG )
        eurotherm902s_set_xs( xs & ~ PID_CONTROL_FLAG );
}


/*-----------------------------------*
 * Returns if there's a sensor break
 *-----------------------------------*/

bool
eurotherm902s_check_sensor_break( void )
{
    return eurotherm902s_get_sw( ) & SENSOR_BREAK_FLAG ? SET : UNSET;

}


/*--------------------------------------------------------------*
 * Sets operation mode (normal operation or configuration mode)
 *--------------------------------------------------------------*/

void
eurotherm902s_set_operation_mode( int op_mode )
{
    char cmd[ ] = "IM *";


    fsc2_assert(    op_mode == NORMAL_OPERATION
                 || op_mode == CONFIGURATION_MODE );

    cmd[ 3 ] = '0' + op_mode;
    bvt3000_send_command( cmd );
}


/*--------------------------------------------------*
 * Returns the operation mode (for normal operation
 * 0 is returned, for configuration mode 2)
 *--------------------------------------------------*/

int
eurotherm902s_get_operation_mode( void )
{
    return T_atoi( bvt3000_query( "IM" ) );
}


/*-----------------------------------------------------------------*
 * Sets the mode to either automatic or manual (in manual mode the
 * output power can be controlled via the "UP" and "DOWN" keys).
 *-----------------------------------------------------------------*/

void
eurotherm902s_set_mode( int mode )
{
    unsigned sw = eurotherm902s_get_sw( );


    fsc2_assert( mode == AUTOMATIC_MODE || mode == MANUAL_MODE );

    if ( mode == AUTOMATIC_MODE )
        sw &= ~ MANUAL_MODE_FLAG;
    else
        sw |= MANUAL_MODE_FLAG;

    eurotherm902s_set_sw( sw );
}


/*---------------------------------------------------------*
 * Returns the mode (automatic or manual) the device is in
 *---------------------------------------------------------*/

int
eurotherm902s_get_mode( void )
{
    return eurotherm902s_get_sw( ) & MANUAL_MODE_FLAG ?
           MANUAL_MODE : AUTOMATIC_MODE;
}


/*--------------------------------------------*
 * Returns the current (measured) temperature
 *--------------------------------------------*/

double
eurotherm902s_get_temperature( void )
{
    return T_atod( bvt3000_query( "PV" ) );
}


/*------------------------------*
 * Sets the setpoint to be used
 *------------------------------*/

void
eurotherm902s_set_active_setpoint( int sp )
{
    unsigned int sw = eurotherm902s_get_sw( );


    fsc2_assert( sp == SP1 || sp == SP2 );

    if ( sp == SP1 )
        sw &= ~ ACTIVE_SETPOINT_FLAG;
    else
        sw |= ACTIVE_SETPOINT_FLAG;

    eurotherm902s_set_sw( sw );
}


/*-------------------------------------*
 * Returns the currently used setpoint
 *-------------------------------------*/

int
eurotherm902s_get_active_setpoint( void )
{
    return eurotherm902s_get_sw( ) & 0x2000 ? SP2 : SP1;
}


/*----------------------------------------------*
 * Sets the setpoint value of either SP1 or SP2
 *----------------------------------------------*/

void
eurotherm902s_set_setpoint( int    sp,
                            double temp )
{
    char buf[ 20 ];

    fsc2_assert( sp == SP1 || sp == SP2 );
    fsc2_assert( sprintf( buf, "S%c%6.1f", sp == SP1 ? 'L' : '2', temp ) <= 8 );
    bvt3000_send_command( buf );
}


/*-------------------------------------------------*
 * Returns the setpoint value of either SP1 or SP2
 *-------------------------------------------------*/

double
eurotherm902s_get_setpoint( int sp )
{
    fsc2_assert( sp == SP1 || sp == SP2 );
    return T_atod( bvt3000_query( sp == SP1 ? "SL" : "S2" ) );
}


/*--------------------------------------------------------*
 * Returns the value of the setpoint currently being used
 * (i.e. the temperature for the currently used setpoint)
 *--------------------------------------------------------*/

double
eurotherm902s_get_working_setpoint( void )
{
    return T_atod( bvt3000_query( "SP" ) );
}


/*----------------------*
 * Sets the status word
 *----------------------*/

void
eurotherm902s_set_sw( unsigned int sw )
{
    char buf[ 8 ];


    fsc2_assert( sw <= 0xFFFF );

    fsc2_assert( sprintf( buf, "SW>%04x", sw & 0xE005 ) == 7 );
    bvt3000_send_command( buf );
}


/*-------------------------*
 * Returns the status word
 *-------------------------*/

unsigned int
eurotherm902s_get_sw( void )
{
    unsigned int sw;
    char *reply = bvt3000_query( "SW" );


    if (    *reply != '>'
         || sscanf( reply + 1, "%x", &sw ) != 1
         || sw > 0xFFFF )
        bvt3000_comm_fail( );
    return sw;
}


/*-------------------------------*
 * Sets the optional status word
 *-------------------------------*/

void
eurotherm902s_set_os( unsigned int os )
{
    char buf[ 8 ];


    fsc2_assert( os <= 0xFFFF );

    fsc2_assert( sprintf( buf, "OS>%04x", os & 0x30BF ) == 7 );
    bvt3000_send_command( buf );
}


/*----------------------------------*
 * Returns the optional status word
 *----------------------------------*/

unsigned int
eurotherm902s_get_os( void )
{
    unsigned int os;
    char *reply = bvt3000_query( "OS" );


    if (    *reply != '>'
         || sscanf( reply + 1, "%x", &os ) != 1
         || os > 0xFFFF )
        bvt3000_comm_fail( );

    return os;
}


/*--------------------------------*
 * Sets the extension status word
 *--------------------------------*/

void
eurotherm902s_set_xs( unsigned int xs )
{
    char buf[ 8 ];


    fsc2_assert( xs <= 0xFFFF );

    fsc2_assert( sprintf( buf, "XS>%04x", xs & 0xFFB7 ) == 7 );
    bvt3000_send_command( buf );
}


/*-----------------------------------*
 * Returns the extension status word
 *-----------------------------------*/

unsigned int
eurotherm902s_get_xs( void )
{
    unsigned int xs;
    char *reply = bvt3000_query( "XS" );


    if (    *reply != '>'
         || sscanf( reply + 1, "%x", &xs ) != 1
         || xs > 0xFFFF )
        bvt3000_comm_fail( );

    return xs;
}


/*---------------------------*
 * Returns if an alarm is on
 *---------------------------*/

bool
eurotherm902s_get_alarm_state( void )
{
    return eurotherm902s_get_xs( ) & ALARMS_STATE_FLAG ? SET : UNSET;
}


/*-------------------------------*
 * Sets self tune stat on or off
 *-------------------------------*/

void
eurotherm902s_set_self_tune_state( bool on_off )
{
    unsigned int xs = eurotherm902s_get_xs( );


    if ( on_off )
        xs |= SELF_TUNE_FLAG;
    else
        xs &= ~ SELF_TUNE_FLAG;

    eurotherm902s_set_xs( xs );
}


/*-----------------------------------------*
 * Returns if device is in self tune state
 *-----------------------------------------*/

bool
eurotherm902s_get_self_tune_state( void )
{
    return eurotherm902s_get_xs( ) & SELF_TUNE_FLAG ? SET : UNSET;
}


/*------------------------------*
 * Sets adaptive tune on or off
 *------------------------------*/

void
eurotherm902s_set_adaptive_tune_state( bool on_off )
{
    unsigned int xs = eurotherm902s_get_xs( );


    if ( on_off )
        xs |= ADAPTIVE_TUNE_FLAG;
    else
        xs &= ~ ADAPTIVE_TUNE_FLAG;

    eurotherm902s_set_xs( xs );
}


/*---------------------------------------*
 * Returns if adaptive tune is on or off
 *---------------------------------------*/

bool
eurotherm902s_get_adaptive_tune_state( void )
{
    return eurotherm902s_get_xs( ) & ADAPTIVE_TUNE_FLAG ? SET : UNSET;
}


/*----------------------------------*
 * Sets adaptive tune trigger level
 *----------------------------------*/

void
eurotherm902s_set_adaptive_tune_trigger( double tr )
{
    char buf[ 20 ];


    fsc2_assert( tr >= 0.0 && tr <= MAX_AT_TRIGGER_LEVEL );

    fsc2_assert( sprintf( buf, "TR%3.2f", tr ) <= 8 );
    bvt3000_send_command( buf );
}


/*-------------------------------------------------*
 * Returns the adaptive tune trigger level (in K?)
 *-------------------------------------------------*/

double
eurotherm902s_get_adaptive_tune_trigger( void )
{
    return T_atod( bvt3000_query( "TR" ) );
}


/*-------------------------------------------------------------------------*
 * Returns the minimum value that can be set for the setpoints SP1 and SP2
 *-------------------------------------------------------------------------*/

double
eurotherm902s_get_min_setpoint( int sp )
{
    fsc2_assert( sp == SP1 || sp == SP2 );
    return T_atod( bvt3000_query( sp == SP1 ? "LS" : "L2" ) );
}


/*-------------------------------------------------------------------------*
 * Returns the maximum value that can be set for the setpoints SP1 and SP2
 *-------------------------------------------------------------------------*/

double
eurotherm902s_get_max_setpoint( int sp )
{
    fsc2_assert( sp == SP1 || sp == SP2 );
    return T_atod( bvt3000_query( sp == SP1 ? "HS" : "H2" ) );
}


/*------------------------------------*
 * Sets the maximum heater power in %
 *------------------------------------*/

void
eurotherm902s_set_heater_power_limit( double power )
{
    char buf[ 20 ];


    fsc2_assert( power >= 0.0 && power <= 100.0 );
    fsc2_assert( sprintf( buf, "HO%6.1f", power ) <= 8 );
    bvt3000_send_command( buf );
}


/*---------------------------------------*
 * Returns the maximum heater power in %
 *---------------------------------------*/

double
eurotherm902s_get_heater_power_limit( void )
{
    return T_atod( bvt3000_query( "HO" ) );
}


/*---------------------------------------*
 * Sets the heater power in % - can only
 * be set when device is in AUTO mode
 *---------------------------------------*/

void
eurotherm902s_set_heater_power( double power )
{
    char buf[ 20 ];


    fsc2_assert( power >= 0.0 && power <= 100.0 );
    fsc2_assert( eurotherm902s_get_mode( ) == AUTOMATIC_MODE );

    fsc2_assert( sprintf( buf, "OP%6.1f", power ) <= 8 );
    bvt3000_send_command( buf );
}


/*-------------------------------*
 * Returns the heater power in %
 *-------------------------------*/

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


    fsc2_assert( pb <= MAX_PROPORTIONAL_BAND );

    fsc2_assert( sprintf( buf, "XP%6.2f", pb ) <= 8 );
    bvt3000_send_command( buf );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
eurotherm902s_get_proportional_band( void )
{
    return T_atod( bvt3000_query( "XP" ) );
}


/*------------------------------------------*
 * Sets the integration time in seconds (?)
 *------------------------------------------*/

void
eurotherm902s_set_integral_time( double it )
{
    char buf[ 20 ];


    fsc2_assert( it <= MAX_INTEGRAL_TIME );

    if ( it < 999.5 )
        fsc2_assert( sprintf( buf, "TI%6.2f", it ) <= 8 );
    else
        fsc2_assert( sprintf( buf, "TI%6.1f", it ) <= 8 );
    bvt3000_send_command( buf );
}


/*---------------------------------------------*
 * Returns the integration time in seconds (?)
 *---------------------------------------------*/

double
eurotherm902s_get_integral_time( void )
{
    return T_atod( bvt3000_query( "TI" ) );
}


/*-----------------------------------------*
 * Sets the derivative time in seconds (?)
 *-----------------------------------------*/

void
eurotherm902s_set_derivative_time( double dt )
{
    char buf[ 20 ];


    fsc2_assert( dt <= MAX_DERIVATIVE_TIME );

    fsc2_assert( sprintf( buf, "TD%6.2f", dt ) <= 8 );
    bvt3000_send_command( buf );
}


/*--------------------------------------------*
 * Returns the derivative time in seconds (?)
 *--------------------------------------------*/

double
eurotherm902s_get_derivative_time( void )
{
    return T_atod( bvt3000_query( "TD" ) );
}


/*-----------------------------*
 * Sets the high cutback value 
 *-----------------------------*/

void
eurotherm902s_set_cutback_high( double cb )
{
    char buf[ 20 ];


    if ( cb < 999.5 )
        fsc2_assert( sprintf( buf, "HB%6.2f", cb ) <= 8 );
    else
        fsc2_assert( sprintf( buf, "HB%6.1f", cb ) <= 8 );
    bvt3000_send_command( buf );
}


/*--------------------------------*
 * Returns the high cutback value 
 *--------------------------------*/

double
eurotherm902s_get_cutback_high( void )
{
    return T_atod( bvt3000_query( "HB" ) );
}


/*----------------------------*
 * Sets the low cutback value 
 *----------------------------*/

void
eurotherm902s_set_cutback_low( double cb )
{
    char buf[ 20 ];


    if ( cb < 999.5 )
        fsc2_assert( sprintf( buf, "LB%6.2f", cb ) <= 8 );
    else
        fsc2_assert( sprintf( buf, "LB%6.1f", cb ) <= 8 );
    bvt3000_send_command( buf );
}


/*-------------------------------*
 * Returns the low cutback value 
 *-------------------------------*/

double
eurotherm902s_get_cutback_low( void )
{
    return T_atod( bvt3000_query( "LB" ) );
}


/*-----------------------------*
 * Returns the display maximum
 *-----------------------------*/

double
eurotherm902s_get_display_maximum( void )
{
    return T_atod( bvt3000_query( "1H" ) );
}


/*-----------------------------*
 * Returns the display minimum
 *-----------------------------*/

double
eurotherm902s_get_display_minimum( void )
{
    return T_atod( bvt3000_query( "1L" ) );
}


/*-------------------------------*
 * Locks or unlocks the keyboard 
 *-------------------------------*/

void
eurotherm902s_lock_keyboard( bool lock )
{
    unsigned int sw = eurotherm902s_get_sw( );


    if ( lock )
        sw |= KEYLOCK_FLAG;
    else
        sw &= ~ KEYLOCK_FLAG;

    eurotherm902s_set_sw( sw );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
