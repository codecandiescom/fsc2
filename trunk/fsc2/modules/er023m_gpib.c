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

static bool dont_print_on_error = UNSET;


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

bool er023m_init( const char *name )
{
	if ( gpib_init_device( name, &er023m.device ) == FAILURE )
	{
		er023m.device = -1;
        return FAIL;
	}

	TRY
	{
		dont_print_on_error = SET;

		/* Make lock-in send its status byte to test that it reacts */

		er023m_sb( );

		/* Switch off service requests */

		er023m_srq( SRQ_OFF );

		/* Set or fetch the receiver gain */

		if ( er023m.rg_index != UNDEF_RG_INDEX )
			er023m_set_rg( er023m.rg_index );
		else
			er023m.rg_index = er023m_get_rg( );

		/* Set or fetch the time constant - make sure it's at least 2.56 ms */

		if ( er023m.tc_index != UNDEF_TC_INDEX )
			er023m_set_tc( er023m.tc_index );
		else
		{
			er023m.tc_index = er023m_get_tc( );
			if ( er023m.tc_index < TC_MIN_INDEX )
			{
				er023m.tc_index = TC_MIN_INDEX;
				er023m_set_tc( er023m.tc_index );
			}
		}

		/* Set the conversion time - if it hasn't been specified by the user
		   set it to a value as near as possible to the time constant */

		if ( er023m.ct_mult != UNDEF_CT_MULT )
			er023m_set_ct( er023m.ct_mult );
		else
		{
			er023m.ct_mult = irnd( er023m.tc_index / BASE_CT );
			if ( er023m.ct_mult < MIN_CT_MULT )
				er023m.ct_mult = MIN_CT_MULT;
			if ( er023m.ct_mult > MAX_CT_MULT )
				er023m.ct_mult = MAX_CT_MULT;
			er023m_set_ct( er023m.ct_mult );
		}

		/* Set or fetch the modulation frequency */

		if ( er023m.mf_index != UNDEF_MF_INDEX )
			er023m_set_mf( er023m.mf_index );
		else
			er023m.mf_index = er023m_get_mf( );

		/* Set or fetch the phase */

		if ( er023m.phase != UNDEF_PHASE )
			er023m_set_ph( er023m.phase );
		else
			er023m.phase = er023m_get_ph( );

		/* Set or fetch the modulation attenuation */

		if ( er023m.ma != UNDEF_MOD_ATT )
			er023m_set_ma( er023m.ma );
		else
			er023m.ma = er023m_get_ma( );

		/* Set or fetch the offset */

		if ( er023m.of != UNDEF_OF )
			er023m_set_of( er023m.of );
		else
			er023m.of = er023m_get_of( );

		/* Set or fetch the harmonic */

		if ( er023m.ha != UNDEF_HARMONIC )
			er023m_set_ha( er023m.ha );
		else
			er023m.ha = er023m_get_ha( );

		/* Set or fetch the resonator */

		if ( er023m.re != UNDEF_RESONATOR )
			er023m_set_re( er023m.re );
		else
			er023m.re = er023m_get_re( );

		/* Set the device to single mode */

		er023m_mode( SINGLE_MODE );

		/* Find out how many data bytes will be send with these settings */
		
		er023m.nb = er023m_get_nb( );

		/* Set ED mode, i.e. show receiver level on display */

		if ( gpib_write( er023m.device, "ED\r", 3 ) == FAILURE )
			return FAIL;

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		dont_print_on_error = UNSET;
		er023m.device = -1;
		return FAIL;
	}

	return OK;
}


/*--------------------------------------------------------------------*/
/* Tis function only works if the data fit into an unsigned int - but */
/* has already been checked when er023m.nb was determined...          */
/*--------------------------------------------------------------------*/

unsigned int er023m_get_data( void )
{
	unsigned char buf[ 30 ];
	long len = 30;
	int i;
	int fac;
	int val = 0;


	/* Bring lock-in into talker mode, this should make it send one ADC data
	   point automatically (when in SINGLE mode) */

	if ( gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	/* The device should send as many bytes for a data point as it told us
	   it would, plus an EOS character */

	if ( len != er023m.nb + 1 )
		er023m_failure( );

	/* The device sends positive numbers only, LSB first */

	for ( val = 0, i = 0, fac = 1; i < er023m.nb; fac *= 0x100, i++ )
		val += fac * ( unsigned int ) buf[ i ];

	return val;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int er023m_get_rg( void )
{
	char buf[ 30 ];
	long len = 30;


	if ( gpib_write( er023m.device, "RG\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len - 1 ] = '\0';
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

	buf[ len - 1 ] = '\0';
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

	buf[ len - 1 ] = '\0';
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

	buf[ len - 1 ] = '\0';
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

	buf[ len - 1 ] = '\0';
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

	buf[ len - 1 ] = '\0';
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

	buf[ len - 1 ] = '\0';
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

	buf[ len - 1 ] = '\0';
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

	buf[ len - 1 ] = '\0';
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

int er023m_get_nb( void )
{
	char buf[ 30 ];
	long len = 30;
	int nb;
	unsigned int fac;
	int i;


	if ( gpib_write( er023m.device, "NB\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len - 1 ] = '\0';
	nb = T_atoi( buf );

	fsc2_assert( nb > 0 && nb <= MAX_NB &&
				 nb <= ( int ) sizeof( unsigned int ) );

	for ( fac = 0, i = 0; i < nb; i++ )
		fac = fac * 256 + 255;

	er023m.scale_factor = 2.0 / ( double ) fac;

	return nb;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void er023m_mode( int mode )
{
	fsc2_assert( mode == SINGLE_MODE || mode == CONTINUOUS_MODE );

	if ( gpib_write( er023m.device,
					 mode == SINGLE_MODE ? "SM\r" : "CM\r", 3 ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void er023m_srq( int on_off )
{
	char buf[ 30 ] = "SR*\r";


	fsc2_assert( on_off == SRQ_OFF || on_off == SRQ_ON );

	buf[ 2 ] = on_off == SRQ_ON ? '1' : '0';
	if ( gpib_write( er023m.device, buf, 4 ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

unsigned char er023m_sb( void )
{
	char buf[ 30 ];
	long len = 30;
	int sb;


	if ( gpib_write( er023m.device, "SB\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len - 1 ] = '\0';
	sb = T_atoi( buf );

	fsc2_assert( sb >= 0 && sb <= 255 );

	return ( unsigned char ) ( sb & 0xFF );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void er023m_failure( void )
{
	if ( ! dont_print_on_error )
		eprint( FATAL, UNSET, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
	THROW( EXCEPTION )
}
