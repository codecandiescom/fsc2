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


#include "er023m.h"


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

Var *lockin_rg( Var *v )
{
	long rg_index;


	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( );

			case TEST :
				return vars_push( INT_VAR, ( long ) 
								  ( er023m.rg_index == UNDEF_RG_INDEX ?
									ER023M_TEST_RG_INDEX : er023m.rg_index ) );

			case EXPERIMENT :
				return vars_push( INT_VAR, ( long ) er023m_get_rg( ) );
		}

	rg_index = get_long( v, "receiver gain index" );

	too_many_arguments( v );

	if ( rg_index < 0 || rg_index > RG_MAX_INDEX )
	{
		print( FATAL, "Invalid receiver gain index %ld, valid range is "
			   "0-%d.\n", rg_index, RG_MAX_INDEX );
		THROW( EXCEPTION );
	}

	er023m.rg_index = ( int ) rg_index;
	if ( FSC2_MODE == EXPERIMENT )
		er023m_set_rg( er023m.rg_index );

	return vars_push( INT_VAR, rg_index );
}


/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/

Var *lockin_tc( Var *v )
{
	long tc_index;


	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( );

			case TEST :
				return vars_push( INT_VAR, ( long ) 
								  ( er023m.tc_index == UNDEF_TC_INDEX ?
									ER023M_TEST_TC_INDEX : er023m.tc_index ) );
			case EXPERIMENT :
				return vars_push( INT_VAR, er023m_get_tc( ) );
		}

	tc_index = get_long( v, "time constant index" );

	if ( tc_index > 0 && tc_index < TC_MIN_INDEX )
	{
		print( SEVERE, "Minimum usuable time constant index is %d, using this "
			   "value instead of %ld.\n", TC_MIN_INDEX, tc_index );
		tc_index = TC_MIN_INDEX;
	}

	if ( tc_index < 0 || tc_index > TC_MAX_INDEX )
	{
		print( FATAL, "Invalid time constant index %ld, valid range is "
			   "%d-%d.\n", tc_index, TC_MIN_INDEX, TC_MAX_INDEX );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	er023m.tc_index = ( int ) tc_index;
	if ( FSC2_MODE == EXPERIMENT )
		er023m_set_tc( er023m.tc_index );

	return vars_push( INT_VAR, tc_index );
}


/*----------------------------------------------------------------------*/
/* Returns or sets the modulation attenuation of the lock-in amplifier. */
/* If called without arguments the modulation attenuation is returned.  */
/* If called with an argument the attenuation is set to this value.     */
/*----------------------------------------------------------------------*/

Var *lockin_ma( Var *v )
{
	long ma;


	if ( er023m.mf_index == UNDEF_MF_INDEX ||
		 ! er023m.calib[ er023m.mf_index ].is_ma )
		print( NO_ERROR, "MA is not calibrated.\n" );

	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( );

			case TEST :
				return vars_push( INT_VAR, ( long )
								  ( er023m.ma_index == UNDEF_MA_INDEX ?
									ER023M_TEST_MA_INDEX : er023m.ma_index ) );

			case EXPERIMENT :
				return vars_push( INT_VAR, ( long ) er023m_get_ma( ) );
		}

	ma = get_long( v, "modulation attenuation index" );

	if ( ma > MAX_MA_INDEX || ma < MIN_MA_INDEX )
	{
		print( FATAL, "Modulation attenuation index %ld is too %s, must be in "
			   "range %d-%d.\n", ma, ma > MAX_MA_INDEX ? "large" : "low",
			   MIN_MA_INDEX, MAX_MA_INDEX );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	er023m.ma_index = ( int ) ma;
	if ( FSC2_MODE == EXPERIMENT )
		er023m_set_ma( er023m.ma_index );

	return vars_push( INT_VAR, ma );
}


/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/

Var *lockin_ct( Var *v )
{
	long ct_mult;


	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( );

			case TEST :
				return vars_push( INT_VAR, ( long )
								  ( er023m.ct_mult == UNDEF_CT_MULT ?
									ER023M_TEST_CT_MULT : er023m.ct_mult ) );

			case EXPERIMENT :
				return vars_push( INT_VAR, ( long ) er023m_get_ct( ) );
		}

	ct_mult = get_long( v, "conversion time multiplicator" );
	if ( ct_mult < 0 )
	{
		print( FATAL, "Invalid negative conversion time multiplier.\n" );
		THROW( EXCEPTION );
	}

	if ( ct_mult < MIN_CT_MULT )
	{
		print( SEVERE, "Conversion time multiplier of %ld is too low, using "
			   "%ld instead.\n", ct_mult, MIN_CT_MULT );
		ct_mult = MIN_CT_MULT;
	}

	if ( ct_mult > MAX_CT_MULT )
	{
		print( SEVERE, "Conversion time multiplier of %ld is too large, using "
			   "%ld instead.\n", ct_mult, MAX_CT_MULT );
		ct_mult = MAX_CT_MULT;
	}

	/* Unfortunately, conversion time multipliers between 125 and 200 lead
	   to garbled data (at least for large signals) so we have to replace
	   conversion time multipliers in this range by the nearest conversion
	   time multiplier that still leads to correct data. */

	if ( ct_mult >= BAD_LOW_CT_MULT && ct_mult < BAD_HIGH_CT_MULT )
	{
		long new_ct_mult;

		if ( ( double ) BAD_LOW_CT_MULT / ( double ) ct_mult >
			 ( double ) ct_mult / ( double ) BAD_HIGH_CT_MULT )
			new_ct_mult = BAD_LOW_CT_MULT - 1;
		else
			new_ct_mult = BAD_HIGH_CT_MULT + 1;

		print( SEVERE, "Conversion time multiplier of %ld might result in "
			   "garbled data, using %ld instead.\n", ct_mult, new_ct_mult );

		ct_mult = new_ct_mult;
	}

	too_many_arguments( v );

	er023m.ct_mult = ( int ) ct_mult;
	if ( FSC2_MODE == EXPERIMENT )
		er023m_set_ct( er023m.ct_mult );

	return vars_push( INT_VAR, ct_mult );
}


/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/

Var *lockin_mf( Var *v )
{
	long mf_index;
	int old_mf_index;


	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( );

			case TEST :
				return vars_push( INT_VAR, ( long )
								  ( er023m.mf_index == UNDEF_MF_INDEX?
									ER023M_TEST_MF_INDEX : er023m.mf_index ) );

			case EXPERIMENT :
				return vars_push( INT_VAR, ( long ) er023m_get_mf( ) );
		}

	old_mf_index = er023m.mf_index;

	mf_index = get_long( v, "modulation frequency index" );

	if ( mf_index < 0 )
	{
		print( FATAL, "Invalid negative modulation frequency index %ld.\n",
			   mf_index );
		THROW( EXCEPTION );
	}

	if ( mf_index > MAX_MF_INDEX )
	{
		print( FATAL, "Invalid modulation frequency index %ld, valid range is "
			   "0-%ld.\n", mf_index, MAX_MF_INDEX );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	er023m.mf_index = ( int ) mf_index;
	if ( FSC2_MODE == EXPERIMENT )
		er023m_set_mf( er023m.mf_index );

	/* Warn the user if for the new modulation frequency there's no phase
	   or attenuation calibration while we had one for the old frequency */

	if ( old_mf_index != UNDEF_MF_INDEX &&
		 old_mf_index != ( int ) mf_index )
	{
		if ( er023m.ha != UNDEF_HARMONIC &&
			 er023m.calib[ old_mf_index ].is_ph[ er023m.ha ] &&
			 ! er023m.calib[ mf_index ].is_ph[ er023m.ha ] )
			print( SEVERE, "Setting new modulation frequency makes phase "
				   "uncalibrated.\n" );
		if ( er023m.calib[ old_mf_index ].is_ma &&
			 ! er023m.calib[ mf_index ].is_ma )
			print( SEVERE, "Setting new modulation frequency makes modulation "
				   "amplitude uncalibrated.\n" );
	}

	return vars_push( INT_VAR, mf_index );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
