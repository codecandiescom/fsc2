/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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


#include "er023m.h"


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int er023m_get_rg( void )
{
	char buf[ 30 ];
	long len = 30;

	if ( gpib_write( er023m.device, "RG\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len ] = '\0';
	return T_atoi( buf );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void er023m_set_rg( int rg_index )
{
	char buf[ 30 ];


	fsc2_assert( rg_index >= 0 && rg_index <= RG_MAX_INDEX );

	sprintf( buf, "RG%d\r", rg_index );
	if ( gpib_write( er023m.device, buf, strlen( buf ) ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int er023m_get_tc( void )
{
	char buf[ 30 ];
	long len = 30;

	if ( gpib_write( er023m.device, "TC\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len ] = '\0';
	return T_atoi( buf );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void er023m_set_tc( int tc_index )
{
	char buf[ 30 ];


	fsc2_assert( tc_index >= TC_MIN_INDEX && tc_index <= TC_MAX_INDEX );

	sprintf( buf, "TC%d\r", tc_index );
	if ( gpib_write( er023m.device, buf, strlen( buf ) ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int er023m_get_ph( void )
{
	char buf[ 30 ];
	long len = 30;

	if ( gpib_write( er023m.device, "PH\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len ] = '\0';
	return T_atoi( buf );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void er023m_set_ph( int ph_index )
{
	char buf[ 30 ];


	fsc2_assert( ph_index >= 0 && ph_index <= 359 );

	sprintf( buf, "PH%d\r", ph_index );
	if ( gpib_write( er023m.device, buf, strlen( buf ) ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int er023m_get_ma( void )
{
	char buf[ 30 ];
	long len = 30;

	if ( gpib_write( er023m.device, "MA\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len ] = '\0';
	return T_atoi( buf );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void er023m_set_ma( int ma )
{
	char buf[ 30 ];


	fsc2_assert( ma >= MIN_MOD_ATT && ma <= MOD_OFF );

	sprintf( buf, "MA%d\r", ma );
	if ( gpib_write( er023m.device, buf, strlen( buf ) ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int er023m_get_of( void )
{
	char buf[ 30 ];
	long len = 30;

	if ( gpib_write( er023m.device, "OF\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len ] = '\0';
	return T_atoi( buf );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void er023m_set_of( int of )
{
	char buf[ 30 ];


	fsc2_assert( of >= MIN_OF && of <= MAX_OF );

	sprintf( buf, "OF%d\r", of );
	if ( gpib_write( er023m.device, buf, strlen( buf ) ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int er023m_get_ct( void )
{
	char buf[ 30 ];
	long len = 30;

	if ( gpib_write( er023m.device, "CT\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len ] = '\0';
	return T_atoi( buf );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void er023m_set_ct( int ct_mult )
{
	char buf[ 30 ];


	fsc2_assert( ct_mult >= MIN_CT_MULT && ct_mult <= MAX_CT_MULT );

	sprintf( buf, "CT%d\r", ct_mult );
	if ( gpib_write( er023m.device, buf, strlen( buf ) ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int er023m_get_mf( void )
{
	char buf[ 30 ];
	long len = 30;

	if ( gpib_write( er023m.device, "MF\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len ] = '\0';
	return T_atoi( buf );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void er023m_set_mf( int mf_index )
{
	char buf[ 30 ];


	fsc2_assert( mf_index >= 0 && mf_index <= MAX_MF_INDEX );

	sprintf( buf, "MF%d\r", mf_index );
	if ( gpib_write( er023m.device, buf, strlen( buf ) ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int er023m_get_ha( void )
{
	char buf[ 30 ];
	long len = 30;

	if ( gpib_write( er023m.device, "HA\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len ] = '\0';
	return T_atoi( buf ) - 1;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void er023m_set_ha( int ha )
{
	char buf[ 30 ];


	fsc2_assert( ha >= MIN_HARMONIC && ha <= MAX_HARMONIC );

	sprintf( buf, "HA%d\r", ha + 1 );
	if ( gpib_write( er023m.device, buf, strlen( buf ) ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int er023m_get_re( void )
{
	char buf[ 30 ];
	long len = 30;

	if ( gpib_write( er023m.device, "RE\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len ] = '\0';
	return T_atoi( buf ) - 1;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void er023m_set_re( int re )
{
	char buf[ 30 ];


	fsc2_assert( re >= MIN_RESONATOR && re <= MAX_RESONATOR );

	sprintf( buf, "RE%d\r", re + 1 );
	if ( gpib_write( er023m.device, buf, strlen( buf ) ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void er023m_failure( void )
{
	eprint( FATAL, UNSET, "%s: Can't access the lock-in amplifier.\n",
			DEVICE_NAME );
	THROW( EXCEPTION )
}
