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


#define ER023M_MAIN

#include "er023m.h"

const char generic_type[ ] = DEVICE_TYPE;

static ER023M er023m_store;


/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int er023m_init_hook( void )
{
	int i;
	double fac;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Device hasn't been assigned a GPIB number yet */

	er023m.device = -1;

	/* Set up the receiver gain list */

	rg_list[ 0 ]  = MIN_RG;
	rg_list[ RG_MAX_INDEX ] = MAX_RG;

	fac = pow( MAX_RG / MIN_RG, 1.0 / ( double ) ( RG_MAX_INDEX ) );

	for ( i = 1; i < RG_MAX_INDEX; i++ )
		rg_list[ i ] = fac * rg_list[ i - 1 ];

	/* Set up the time constant list */

	tc_list[ 0 ] = MIN_TC;
	for ( i = 1; i <= TC_MAX_INDEX; i++ )
		tc_list[ i ] = 2.0 * tc_list[ i - 1 ];

	/* Set up the modulation frequency list */

	mf_list[ 0 ] = MAX_MOD_FREQ;
	for ( i = 1; i <= MAX_MF_INDEX; i++ )
		mf_list[ i ] = 0.5 * mf_list[ i - 1 ];

	/* Clear the calibration list */

	for ( i = 0; i <= MAX_MF_INDEX; i++ )
		er023m.calib[ i ].is_ph[ 0 ] = 
			er023m.calib[ i ].is_ph[ 1 ] = 
				er023m.calib[ i ].is_ma = UNSET;

	/* Set several more variables in the structure describing the device */

	er023m.rg_index = UNDEF_RG_INDEX;
	er023m.tc_index = UNDEF_TC_INDEX;
	er023m.phase    = UNDEF_PHASE;
	er023m.mf_index = UNDEF_MF_INDEX;
	er023m.ma       = UNDEF_MOD_ATT;
	er023m.of       = UNDEF_OF;
	er023m.ha       = UNDEF_HARMONIC;
	er023m.ct_mult  = UNDEF_CT_MULT;
	er023m.re       = UNDEF_RESONATOR;

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int er023m_exp_hook( void )
{
	/* Store the current state and initialize the lock-in */

	memcpy( &er023m_store, &er023m, sizeof( ER023M ) );

	if ( ! er023m_init( DEVICE_NAME ) )
	{
		eprint( FATAL, UNSET, "%s: Initialization of device failed: %s\n",
				DEVICE_NAME, gpib_error_msg );
		THROW( EXCEPTION )
	}

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int er023m_end_of_exp_hook( void )
{
	/* Switch lock-in back to local mode */

	if ( er023m.device >= 0 )
		gpib_local( er023m.device );

	memcpy( &er023m, &er023m_store, sizeof( ER023M ) );
	er023m.device = -1;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *lockin_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------------------------*/
/* Function returns a lock-in data value, scaled to the interval [-1:1] */
/*----------------------------------------------------------------------*/

Var *lockin_get_data( Var *v )
{
	double val;


	if ( v != NULL )
		eprint( WARN, SET, "%s: Useless parameter%s in call of %s().\n",
				DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

	if ( TEST_RUN )                  /* return dummy value in test run */
		return vars_push( FLOAT_VAR, ER023M_TEST_DATA );

	val = ( ( double ) ( er023m_get_data( ) - er023m.min )
			* er023m.scale_factor - 1.0 ) / rg_list[ er023m.rg_index ];
	return vars_push( FLOAT_VAR, val );
}


/*-----------------------------------------------------------------------*/
/* Returns or sets sensitivity (receiver gain) of the lock-in amplifier. */
/* If called with no argument the current receiver gain is returned,     */
/* otherwise the receiver gain is set to the argument. Possible receiver */
/* gain settings are in the range between 2.0e1 and 1.0e7 in steps of    */
/* about 1, 1.25, 1.5, 2, 2.5, 3.0, 4.0, 5.0, 6.0 and 8.0 (multiplied by */
/* a power of 10).                                                       */
/*-----------------------------------------------------------------------*/

Var *lockin_sensitivity( Var *v )
{
	double rg;
	int rg_index = UNDEF_RG_INDEX;
	int i;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( FLOAT_VAR,
						   rg_list[ er023m.rg_index == UNDEF_RG_INDEX ?
									ER023M_TEST_RG_INDEX : er023m.rg_index ] );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
			return vars_push( FLOAT_VAR, rg_list[ er023m_get_rg( ) ] );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, SET, "%s: Integer value used as receiver gain.\n",
				DEVICE_NAME );
	rg = VALUE( v );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( rg <= 0.0 )
	{
		eprint( FATAL, SET, "%s: Negative or zero receiver gain in %s().\n",
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	/* Try to match the receiver gain passed to the function by checking if it
	   fits in between two of the valid values and setting it to the nearer
	   one and, if this doesn't work, set it to the minimum or maximum value,
	   depending on the size of the argument. If the value does not fit within
	   6 percent, utter a warning message */

	for ( i = 0; i < RG_MAX_INDEX; i++ )
		if ( rg >= rg_list[ i ] && rg <= rg_list[ i + 1 ] )
		{
			rg_index = i
			   + ( ( rg_list[ i ] / rg > rg / rg_list[ i + 1 ] ) ? 0 : 1 );
			break;
		}

	if ( rg_index == UNDEF_RG_INDEX )
	{
		if ( rg / rg_list[ RG_MAX_INDEX ] <= 1.06 )
			rg_index = RG_MAX_INDEX;
		else if ( rg_list[ 0 ] / rg <= 1.06 )
			rg_index = 0;
		else
		{
			if ( rg < rg_list[ 0 ] )
				rg_index = 0;
			else
				rg_index = RG_MAX_INDEX;

			eprint( WARN, SET, "%s: Receiver gain of %.2e is too %s, using "
					"%.2e instead.\n", DEVICE_NAME, rg,
					rg_index == 0 ? "low" : "high", rg_list[ rg_index ] );
		}
	}
	else if ( fabs( rg - rg_list[ rg_index ] ) > rg * 6.0e-2 )
		eprint( WARN, SET, "%s: Can't set receiver gain to %.2e, using %.2e "
				"instead.\n", DEVICE_NAME, rg, rg_list[ rg_index ] );

	if ( ! TEST_RUN )
	{
		er023m.rg_index = rg_index;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_rg( rg_index );
	}
	
	return vars_push( FLOAT_VAR, rg_list[ rg_index ] );
}


/*------------------------------------------------------------------------*/
/* Returns or sets the time constant of the lock-in amplifier. If called  */
/* without an argument the time constant is returned (in secs). If called */
/* with an argument the time constant is set to this value. The minimum   */
/* usuable time constant is 2.56 ms.                                      */
/*------------------------------------------------------------------------*/

Var *lockin_time_constant( Var *v )
{
	double tc;
	int tc_index = UNDEF_TC_INDEX;
	int i;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( FLOAT_VAR,
						   tc_list[ er023m.tc_index == UNDEF_TC_INDEX ?
									ER023M_TEST_TC_INDEX : er023m.tc_index ] );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
			return vars_push( FLOAT_VAR, tc_list[ er023m_get_tc( ) ] );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, SET, "%s: Integer value used as time constant.\n",
				DEVICE_NAME );
	tc = VALUE( v );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( tc <= 0.0 )
	{
		eprint( FATAL, SET, "%s: Negative or zero time constant in %s().\n",
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	/* We try to match the time constant passed to the function by checking if
	   it fits in between two of the valid values and setting it to the nearer
	   one and, if this doesn't work, we set it to the minimum or maximum
	   value, depending on the size of the argument. If the value does not fit
	   within 6 percent, we utter a warning message */
	
	for ( i = 0; i < TC_MAX_INDEX; i++ )
		if ( tc >= tc_list[ i ] && tc <= tc_list[ i + 1 ] )
		{
			tc_index = i
				   + ( ( tc / tc_list[ i ] < tc_list[ i + 1 ] / tc ) ? 0 : 1 );
			break;
		}

	if ( tc_index == UNDEF_TC_INDEX )
	{
		if ( tc_list[ 0 ] / tc < 1.06 )
			tc_index = 0;
		if ( tc / tc_list[ TC_MAX_INDEX ] < 1.06 )
			tc_index = TC_MAX_INDEX;
	}

	if ( tc_index != UNDEF_TC_INDEX)                        /* value found ? */
	{
		if ( tc_index < TC_MIN_INDEX )
		{
			eprint( SEVERE, SET, "%s: Time constants below %.2f ms can't be "
					"used, increasing time constant to this value.\n",
					DEVICE_NAME, 1.0e3 * tc_list[ TC_MIN_INDEX ] );
			tc_index = TC_MIN_INDEX;
		}
		else if ( fabs( tc - tc_list[ tc_index ] ) > tc * 6.0e-2 )
		{                                                   /* error > 6% ? */
			if ( tc > 1.0 )
				eprint( WARN, SET, "%s: Can't set time constant to %.2f s, "
						"using %.2f s instead.\n", DEVICE_NAME, tc,
						tc_list[ tc_index ] );
			else
				eprint( WARN, SET, "%s: Can't set time constant to %.2f ms,"
						" using %.2f ms instead.\n", DEVICE_NAME,
						tc * 1.0e3, tc_list[ tc_index ] * 1.0e3 );
		}
	}
	
	if ( tc_index < 0 )                                  /* not found yet ? */
	{
		if ( tc < tc_list[ 0 ] )
			tc_index = TC_MIN_INDEX;
		else
			tc_index = TC_MAX_INDEX;

		eprint( WARN, SET, "%s: Time constant of %.2f s is too %s, using %.2f "
				"s instead.\n",	DEVICE_NAME, tc,
				tc_index == 0 ? "short" : "long", tc_list[ tc_index ] );
	}

	if ( ! TEST_RUN )
	{
		er023m.tc_index = tc_index;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_tc( tc_index );
			
	}
	
	return vars_push( FLOAT_VAR, tc_list[ tc_index ] );
}


/*-----------------------------------------------------------------------*/
/* Returns or sets the phase of the lock-in amplifier. If called without */
/* an argument the phase is returned (range 0-359). If called with an    */
/* argument the phase is set to this value (after mapping it into the    */
/* 0-359 range).                                                         */
/*-----------------------------------------------------------------------*/

Var *lockin_phase( Var *v )
{
	int phase;


	if ( er023m.mf_index == UNDEF_MF_INDEX ||
		 ! er023m.calib[ er023m.mf_index ].is_ph )
		eprint( WARN, SET, "%s: Phase is not calibrated.\n", DEVICE_NAME );

	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( FLOAT_VAR, ( double )
							  ( er023m.phase == UNDEF_PHASE ?
								ER023M_TEST_PHASE : er023m.phase ) );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
			return vars_push( FLOAT_VAR, ( double ) er023m_get_ph( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		phase = ( int ) v->val.lval;
	else
		phase = irnd( v->val.dval );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	while ( phase < 0 )
		phase += 360;
	while ( phase > 359 )
		phase -= 360;

	if ( ! TEST_RUN )
	{
		er023m.phase = phase;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_ph( phase );
	}
	
	return vars_push( FLOAT_VAR, ( double ) phase );
}


/*------------------------------------------------------------------------*/
/* Returns or sets the offset of the lock-in amplifier. If called without */
/* an argument the offset is returned (range 0-99). If called with an     */
/* argument the offset is set to this value.                              */
/*------------------------------------------------------------------------*/

Var *lockin_offset( Var *v )
{
	int of;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( INT_VAR, ( long )
							  ( er023m.of == UNDEF_OF ?
								ER023M_TEST_OF : er023m.of ) );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
			return vars_push( INT_VAR, ( long ) er023m_get_of( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )
	{
		eprint( WARN, SET, "%s: Floating point value used as offset in "
				"%s().\n", DEVICE_NAME, Cur_Func );
		of = irnd( v->val.dval );
	}
	else
		of = ( int ) v->val.lval;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( of < MIN_OF || of > MAX_OF )
	{
		eprint( FATAL, SET, "%s: Invalid offset value %d in %s(), must be in "
				"range %d-%d.\n", DEVICE_NAME, of, Cur_Func, MIN_OF, MAX_OF );
		THROW( EXCEPTION )
	}

	if ( ! TEST_RUN )
	{
		er023m.of = of;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_of( of );
	}
	
	return vars_push( INT_VAR, ( long ) of );
}


/*------------------------------------------------------------------------*/
/* Returns or sets the conversion time. If called without an argument the */
/* conversion time is returned (in secs). If called with an argument the  */
/* conversion time is set to this value. The minimum usuable conversion   */
/* time constant is 3.2 ms.                                               */
/*------------------------------------------------------------------------*/

Var *lockin_conversion_time( Var *v )
{
	double ct;
	int ct_mult;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( FLOAT_VAR, BASE_CT * 
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
			return vars_push( FLOAT_VAR, BASE_CT * er023m_get_ct( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, SET, "%s: Integer value used as conversion time.\n",
				DEVICE_NAME );
	ct = VALUE( v );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( ct <= 0.0 )
	{
		eprint( FATAL, SET, "%s: Negative or zero conversion time in %s().\n",
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	ct_mult = irnd( ct / BASE_CT );

	if ( ct_mult < MIN_CT_MULT )
	{
		eprint( SEVERE, SET, "%s: Conversion time too short, using %.2f ms.\n",
				DEVICE_NAME, 1.0e3 * MIN_CT_MULT * BASE_CT );
		ct_mult = MIN_CT_MULT;
	}

	if ( ct_mult > MAX_CT_MULT )
	{
		eprint( SEVERE, SET, "%s: Conversion time too long, using %.2f s.\n",
				DEVICE_NAME, MAX_CT_MULT * BASE_CT );
		ct_mult = MAX_CT_MULT;
	}

	if ( ! TEST_RUN )
	{
		er023m.ct_mult = ct_mult;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_ct( ct_mult );
			
	}
	
	return vars_push( FLOAT_VAR, ct_mult * BASE_CT );
}


/*--------------------------------------------------------------------------*/
/* Returns or sets the modulation frequency. If called without an argument  */
/* the modulation frequency is returned (in Hz). If called with an argument */
/* the modulation frequency is set to this value.                           */
/*--------------------------------------------------------------------------*/

Var *lockin_ref_freq( Var *v )
{
	double mf;
	int mf_index = UNDEF_MF_INDEX;
	int old_mf_index;
	int i;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( FLOAT_VAR,
						   mf_list[ er023m.mf_index == UNDEF_MF_INDEX ?
								    ER023M_TEST_MF_INDEX : er023m.mf_index ] );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
			return vars_push( FLOAT_VAR, mf_list[ er023m_get_mf( ) ] );
		}
	}

	old_mf_index = er023m.mf_index;

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, SET, "%s: Integer value used as modulation frequency.\n",
				DEVICE_NAME );
	mf = VALUE( v );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( mf <= 0.0 )
	{
		eprint( FATAL, SET, "%s: Negative or zero modulation frequency in "
				"%s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	/* Find a valid modulation frequency nearest to the one we got */

	for ( i = 0; i < MAX_MF_INDEX; i++ )
		if ( mf <= mf_list[ i ] && mf > mf_list[ i + 1 ] )
		{
			mf_index = i
				   + ( ( mf / mf_list[ i ] > mf_list[ i + 1 ] / mf ) ? 0 : 1 );
			break;
		}

	if ( mf_index == UNDEF_MF_INDEX )
	{
		if ( mf > mf_list[ 0 ] )
			mf_index = 0;
		else
			mf_index = MAX_MF_INDEX;

		eprint( WARN, SET, "%s: Modulation frequency of %.2f kHz is too "
				"%s, using %.2f kHz instead.\n",
				DEVICE_NAME, mf * 1.0e-3, mf_index == 0 ? "high" : "low",
				mf_list[ mf_index ] * 1.0e-3 );
	}
	else if ( fabs( mf - mf_list[ mf_index ] ) > mf * 6.0e-2 )
		eprint( WARN, SET, "%s: Can't set modulation frequency to %.2f kHz, "
				"using %.2f kHz instead.\n", DEVICE_NAME, mf * 1.0e-3,
				mf_list[ mf_index ] * 1.0e-3 );

	if ( ! TEST_RUN )
	{
		er023m.mf_index = mf_index;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
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

	return vars_push( FLOAT_VAR, mf_list[ mf_index ] );
}


/*----------------------------------------------------------------------*/
/* Returns or sets the harmonic to be used by the lock-in amplifier. If */
/* called without an argument the harmonic is returned (1 or 2). If     */
/* called with an argument the harmonic is set to this value.           */
/*----------------------------------------------------------------------*/

Var *lockin_harmonic( Var *v )
{
	int ha;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( INT_VAR, 1L + ( long )
							  ( er023m.ha == UNDEF_HARMONIC ?
								ER023M_TEST_HARMONIC : er023m.ha ) );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
			return vars_push( INT_VAR, ( long ) ( er023m_get_ph( ) + 1 ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )
	{
		eprint( WARN, SET, "%s: Float value used as harmonic.\n",
				DEVICE_NAME );
		ha = irnd( v->val.dval ) - 1;
	}
	else
		ha = ( int ) v->val.lval - 1;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( ha < MIN_HARMONIC || ha > MAX_HARMONIC )
	{
		eprint( FATAL, SET, "%s: Invalid value %d used for harmonic, valid "
				"values are %d and %d.\n", DEVICE_NAME, ha + 1,
				MIN_HARMONIC + 1, MAX_HARMONIC + 1 );
		THROW( EXCEPTION )
	}

	if ( ! TEST_RUN )
	{
		er023m.ha = ha;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_ha( ha );
	}
	
	return vars_push( INT_VAR, ( long ) ( ha + 1 ) );
}


/*-----------------------------------------------------------------------*/
/* Returns or sets the resonator to be used by the lock-in amplifier. If */
/* called without an argument the resonator is returned (1 or 2). If     */
/* called with an argument the resonator is set to this value.           */
/*-----------------------------------------------------------------------*/

Var *lockin_resonator( Var *v )
{
	int re;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( INT_VAR, 1L + ( long )
							  ( er023m.re == UNDEF_RESONATOR ?
								ER023M_TEST_RESONATOR : er023m.re ) );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
			return vars_push( INT_VAR, ( long ) ( er023m_get_re( ) + 1 ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )
	{
		eprint( WARN, SET, "%s: Float value used as resonator number.\n",
				DEVICE_NAME );
		re = irnd( v->val.dval ) - 1;
	}
	else
		re = ( int ) v->val.lval - 1;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( re < MIN_RESONATOR || re > MAX_RESONATOR )
	{
		eprint( FATAL, SET, "%s: Invalid value %d used as resonator number, "
				"valid values are %d and %d.\n", DEVICE_NAME, re + 1,
				MIN_RESONATOR + 1, MAX_RESONATOR + 1 );
		THROW( EXCEPTION )
	}

	if ( ! TEST_RUN )
	{
		er023m.re = re;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_re( re );
	}
	
	return vars_push( INT_VAR, ( long ) ( re + 1 ) );
}


/*-----------------------------------------------------------------------*/
/* Function returns an integer variable with a value of 1 if the for the */
/* last data fetched from the device an overload occurred, otherwise 0.  */
/*-----------------------------------------------------------------------*/

Var *lockin_is_overload( Var *v )
{
	long res;


	v = v;

	if ( TEST_RUN )
		return vars_push( INT_VAR, 0 );

	if ( er023m.st_is_valid )
		return vars_push( INT_VAR, ( long ) ( er023m.st & 1 ) );

	res = ( long ) ( er023m_st( ) & 1 );
	er023m.st_is_valid = SET;

	return vars_push( INT_VAR, res );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
