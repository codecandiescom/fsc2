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


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

Var *lockin_rg( Var *v )
{
	int rg_index;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( INT_VAR, ( long ) 
							  ( er023m.rg_index == UNDEF_RG_INDEX ?
								ER023M_TEST_RG_INDEX : er023m.rg_index ) );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
			return vars_push( INT_VAR, ( long ) er023m_get_rg( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )
	{
		eprint( WARN, SET, "%s: Float value used as receiver gain index.\n",
				DEVICE_NAME );
		rg_index = irnd( v->val.dval );
	}
	else
		rg_index = ( int ) v->val.lval;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( rg_index < 0 || rg_index > RG_MAX_INDEX )
	{
		eprint( FATAL, SET, "%s: Invalid receiver gain index %d in %s(), "
				"valid range is 0-%d.\n",
				DEVICE_NAME, rg_index, RG_MAX_INDEX );
		THROW( EXCEPTION )
	}

	if ( ! TEST_RUN )
	{
		er023m.rg_index = rg_index;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_rg( rg_index );
	}
	
	return vars_push( INT_VAR, ( long ) rg_index );
}


/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/

Var *lockin_tc( Var *v )
{
	int tc_index;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( INT_VAR, ( long ) 
							  ( er023m.tc_index == UNDEF_TC_INDEX ?
								ER023M_TEST_TC_INDEX : er023m.tc_index ) );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
			return vars_push( INT_VAR, er023m_get_tc( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )

	{
		eprint( WARN, SET, "%s: Float value used as time constant index.\n",
				DEVICE_NAME );
		tc_index =  irnd( v->val.dval );
	}
	else
		tc_index = ( int ) v->val.lval;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}
	
	if ( tc_index > 0 && tc_index < TC_MIN_INDEX )
	{
		eprint( SEVERE, SET, "%s: Minimum usuable time constant index is %d, "
				"using this value instead of %d.\n", DEVICE_NAME, TC_MIN_INDEX,
				tc_index );
		tc_index = TC_MIN_INDEX;
	}

	if ( tc_index < 0 || tc_index > TC_MAX_INDEX )
	{
		eprint( FATAL, SET, "%s: Invalid time constant index %d in %s(), "
				"valid range is %d-%d.\n",
				DEVICE_NAME, tc_index, TC_MIN_INDEX, TC_MAX_INDEX );
		THROW( EXCEPTION )
	}

	if ( ! TEST_RUN )
	{
		er023m.tc_index = tc_index;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_tc( tc_index );
	}
	
	return vars_push( INT_VAR, ( long ) tc_index );
}


/*----------------------------------------------------------------------*/
/* Returns or sets the modulation attenuation of the lock-in amplifier. */
/* If called without arguments the modulation attenuation is returned.  */
/* If called with an argument the attenuation is set to this value.     */
/*----------------------------------------------------------------------*/

Var *lockin_ma( Var *v )
{
	int ma;


	if ( er023m.mf_index == UNDEF_MF_INDEX ||
		 ! er023m.calib[ er023m.mf_index ].is_ma )
		eprint( WARN, SET, "%s: MA is not calibrated.\n", DEVICE_NAME );

	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( INT_VAR, ( long )
							  ( er023m.ma == UNDEF_MOD_ATT ?
								ER023M_TEST_MOD_ATT : er023m.ma ) );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
			return vars_push( INT_VAR, (long ) er023m_get_ma( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )
	{
		eprint( WARN, SET, "%s: Float value used as modulation attenuation "
				"index in %s().\n", DEVICE_NAME, Cur_Func );
		ma = irnd( v->val.dval );
	}
	else
		ma = ( int ) v->val.lval;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( ma > MOD_OFF || ma < MIN_MOD_ATT )
	{
		eprint( FATAL, SET, "%s: Value %d for modulation attenuation is too "
				"%s, must be in range %d-%d.\n", DEVICE_NAME, ma,
				ma > MOD_OFF ? "large" : "low", MIN_MOD_ATT, MOD_OFF );
		THROW( EXCEPTION )
	}

	if ( ! TEST_RUN )
	{
		er023m.ma = ma;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_ma( ma );
			
	}
	
	return vars_push( INT_VAR, ( long ) ma );
}


/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/

Var *lockin_ct( Var *v )
{
	int ct_mult;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( INT_VAR, ( long )
							  ( er023m.ct_mult == UNDEF_CT_MULT ?
								ER023M_TEST_CT_MULT : er023m.ct_mult ) );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
			return vars_push( INT_VAR, ( long ) er023m_get_ct( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )

	{
		eprint( WARN, SET, "%s: Float value used as conversion time "
				"multiplicator.\n", DEVICE_NAME );
		ct_mult = irnd( v->val.dval );
	}
	else
		ct_mult = ( int ) v->val.lval;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}
	
	if ( ct_mult < 0 )
	{
		eprint( FATAL, SET, "%s: Invalid negative conversion time multiplier "
				"in %s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	if ( ct_mult < MIN_CT_MULT )
	{
		eprint( SEVERE, SET, "%s: Minimum usuable conversion time multiplier "
				"is %d, using this value instead of %d.\n", DEVICE_NAME,
				TC_MIN_INDEX, ct_mult );
		ct_mult = MIN_CT_MULT;
	}

	if ( ct_mult > MAX_CT_MULT )
	{
		eprint( FATAL, SET, "%s: Invalid conversion time multiplier %d in "
				"%s(), valid range is %d-%ld.\n", DEVICE_NAME, ct_mult,
				Cur_Func, MIN_CT_MULT, MAX_CT_MULT );
		THROW( EXCEPTION )
	}

	if ( ! TEST_RUN )
	{
		er023m.ct_mult = ct_mult;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_tc( ct_mult );
	}
	
	return vars_push( INT_VAR, ( long ) ct_mult );
}


/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/

Var *lockin_mf( Var *v )
{
	int mf_index;
	int old_mf_index;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( INT_VAR, ( long )
							  ( er023m.mf_index == UNDEF_MF_INDEX?
								ER023M_TEST_MF_INDEX : er023m.mf_index ) );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
			return vars_push( INT_VAR, ( long ) er023m_get_mf( ) );
		}
	}

	old_mf_index = er023m.mf_index;

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )

	{
		eprint( WARN, SET, "%s: Float value used as modulation frequency "
				"index.\n", DEVICE_NAME );
		mf_index = irnd( v->val.dval );
	}
	else
		mf_index = ( int ) v->val.lval;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}
	
	if ( mf_index < 0 )
	{
		eprint( FATAL, SET, "%s: Invalid negative modulation frequency index "
				"in %s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	if ( mf_index > MAX_MF_INDEX )
	{
		eprint( FATAL, SET, "%s: Invalid modulation frequency index %d in "
				"%s(), valid range is 0-%ld.\n", DEVICE_NAME, mf_index,
				Cur_Func, MAX_MF_INDEX );
		THROW( EXCEPTION )
	}

	if ( ! TEST_RUN )
	{
		er023m.mf_index = mf_index;
		if ( I_am == CHILD )              /* if called in EXPERIMENT section */
			er023m_set_mf( mf_index );
	}
	
	/* Warn the user if for the new modulation frequency there's no phase
	   or attenuation calibration while we had one for the old frequency */

	if ( old_mf_index != UNDEF_MF_INDEX &&
		 old_mf_index != mf_index )
	{
		if ( er023m.ha != UNDEF_HARMONIC &&
			 er023m.calib[ old_mf_index ].is_ph[ er023m.ha ] &&
			 ! er023m.calib[ mf_index ].is_ph[ er023m.ha ] )
			eprint( SEVERE, SET, "%s: Setting new modulation frequency makes "
					"phase uncalibrated.\n", DEVICE_NAME );
		if ( er023m.calib[ old_mf_index ].is_ma &&
			 ! er023m.calib[ mf_index ].is_ma )
			eprint( SEVERE, SET, "%s: Setting new modulation frequency makes "
					"modulation amplitude uncalibrated.\n", DEVICE_NAME );
	}

	return vars_push( INT_VAR, ( long ) mf_index );
}
