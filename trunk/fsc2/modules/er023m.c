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


#include "fsc2.h"
#include "gpib_if.h"


/* Include configuration information for the device */

#include "er023m.conf"

const char generic_type[ ] = DEVICE_TYPE;



/* Here values are defined that get returned by the driver in the test run
   when the lock-in can't be accessed - these values must really be
   reasonable ! */

#define ER023M_TEST_RG_INDEX      30        /* receiver gain 2.0e5 */
#define ER023M_TEST_TC_INDEX      15        /* time constant ~ 300 ms */
#define ER023M_TEST_PHASE         0
#define ER023M_TEST_MF_INDEX      1         /* 50 kHz */
#define ER023M_TEST_MOD_ATT       40


#define RG_MAX_INDEX          57
#define MIN_RG                2.0e1
#define MAX_RG                1.0e7
#define UNDEF_RG_INDEX        -1

static double rg_list[ RG_MAX_INDEX + 1 ];


#define TC_MAX_INDEX          19
#define MAX_TC                5.24288
#define MIN_TC                1.0e-5
#define UNDEF_TC_INDEX        -1

static double tc_list[ TC_MAX_INDEX + 1 ];


#define MAX_MF_INDEX          6
#define MAX_MOD_FREQ          100000.0
#define MIN_MOD_FREQ          0.001
#define UNDEF_MF_INDEX        -1

static double mod_freq_list[ ] = { 1.0e5, 5.0e4, 2.5e4, 1.25e4, 6.25e3,
								   3.125e3, 1.5625e3 };


#define MAX_MOD_ATT           79
#define MIN_MOD_ATT           0
#define MOD_OFF               80
#define UNDEF_MOD_ATT         -1

#define MAX_HARMONIC          2
#define MIN_HARMONIC          1
#define UNDEF_HARMONIC        -1

#define MIN_OF                0
#define MAX_OF                99
#define CENTER_OF             50


/* Declaration of exported functions */

int er023m_init_hook( void );
int er023m_exp_hook( void );
int er023m_end_of_exp_hook( void );
void er023m_exit_hook( void );

Var *lockin_sensitivity( Var *v );
Var *lockin_time_constant( Var *v );
Var *lockin_phase( Var *v );
Var *lockin_ref_level( Var *v );

Var *lockin_rg( Var *v );
Var *lockin_tc( Var *v );


static int er023m_get_rg( void );
static void er023m_set_rg( int rg_index );
static int er023m_get_tc( void );
static void er023m_set_tc( int tc_index );
static int er023m_get_ph( void );
static void er023m_set_ph( int ph_index );
static int er023m_get_ma( void );
static void er023m_set_ma( int ma );
static void er023m_failure( void );



typedef struct {
	int device;

	int rg_index;         /* receiver gain index */
	int tc_index;         /* time constant index */
	int mf_index;         /* modulation frequency index */
	int phase;            /* modulation phase */
	int ma;               /* modulation attenuation */
	int harmonic;         /* harmonic setting */

	int of;
	bool is_of;

	int ct;
	bool is_ct;

	int nb;               /* number of bytes send from ADC */
	                      /* recheck whenever CT changes */
} ER023M;


static ER023M er023m;
static ER023M er023m_store;




/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int er023m_init_hook( void )
{
	int i;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Reset several variables in the structure describing the device */

	er023m.device = -1;

	/* Set up the receiver gain list */

	rg_list[ 0 ]  = MIN_RG;
	rg_list[ RG_MAX_INDEX ] = MAX_RG;
	fac = pow( MAX_RG / MIN_RG, 1.0 / ( double ) ( RC_SETTINGS - 1 ) );
	for ( i = 1; i < RG_MAX_INDEX; i++ )
		rg_list[ i ] = fac * rg_list[ i - 1 ];

	/* Set up the time constant list */

	tc_list[ 0 ] = MIN_TC;
	for ( i = 1; i <= TC_MAX_INDEX; i++ )
		tc_list[ i ] = 2.0 * tc_list[ i - 1 ];

	er023m.rg_index = UNDEF_RG_INDEX;
	er023m.tc_index = UNDEF_TC_INDEX;
	er023m.is_phase = UNDEF_PHASE;
	er023m.mf_index = UNDEF_MF_INDEX;
	er023m.ma       = UNDEF_MOD_ATT;
	er023m.harmonic = UNDEF_HARMONIC;
	er023m.is_of = UNSET;        /* no offset has been set yet */
	er023m.is_ct = UNSET;        /* no conversion time has been set yet */

	return 1;
}


/*-----------------------------------------------------------------------*/
/* Returns or sets sensitivity (receiver gain) of the lock-in amplifier. */
/* If called with no argument the current receiver gain is returned,     */
/* otherwise the receiver gain is set to the argument. Possible receiver */
/* gain settings are in the range between 2.0e1 and 1.0e7 in steps of    */
/* about 1, 1.25, 1.5, 2, 2.5, 3.0, 4.0, 5.0, 6.0 and 8.0 (multiplied by */
/* a power of 10). 
/*-----------------------------------------------------------------------*/

Var *lockin_sensitivity( Var *v )
{
	double rg;
	int rg_index = -1;
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

	if ( rg_index < 0 && rg < rg_list[ RG_MAX_INDEX ] * 1.06 )
		rg_index = RG_MAX_INDEX;

	if ( rg_index >= 0 &&                                 /* value found ? */
		 fabs( rg - rg_list[ rg_index ] ) > rg * 6.0e-2 )   /* error > 6% ? */
		eprint( WARN, SET, "%s: Can't set receiver gain to %.2e, using %.2e "
				"instead.\n", DEVICE_NAME, rg, rg_list[ rg_index ] );

	if ( rg_index < 0 )                                   /* not found yet ? */
	{
		if ( rg < rg_list[ 0 ] )
			rg_index = 0;
		else
		    rg_index = RG_MAX_INDEX;

		eprint( WARN, SET, "%s: Receiver gain of %.2e is too %s, using "
				"%.2e instead.\n", DEVICE_NAME, rg,
				rg_index == 0 ? "low" : "high"rg_list[ rg_index ] );
	}

	if ( ! TEST_RUN )
	{
		er023m.rg_index = rg_index;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_rg( rg_index );
	}
	
	return vars_push( FLOAT_VAR, rg_list[ rg_index ] );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

Var *lockin_rg( Var *v )
{
	long rg_index;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( INT_VAR, er023m.rg_index == UNDEF_RG_INDEX ?
							  ER023M_TEST_RG_INDEX : er023m.rg_index );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Function );
				THROW( EXCEPTION )
			}
			return vars_push( INT_VAR, er023m_get_rg( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )
	{
		eprint( WARN, SET, "%s: Float value used as receiver gain index.\n",
				DEVICE_NAME );
		rg_index = lrnd( v->val.dval );
	}
	else
		rg_index = v->val.lval;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( rg_index < 0 || rg_index > RG_MAX_INDEX )
	{
		eprint( FATAL, SET, "%s: Invalid receiver gain index %ld in %s(), "
				"valid range is 0-%ld.\n",
				DEVICE_NAME, rg_index, RG_MAX_INDEX );
		THROW( EXCEPTION )
	}

	if ( ! TEST_RUN )
	{
		er023m.rg_index = rg_index;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_rg( rg_index );
	}
	
	return vars_push( INT_VAR, rg_index );
}


/*------------------------------------------------------------------------*/
/* Returns or sets the time constant of the lock-in amplifier. If called  */
/* without an argument the time constant is returned (in secs). If called */
/* with an argument the time constant is set to this value.               */
/*------------------------------------------------------------------------*/

Var *lockin_time_constant( Var *v )
{
	double tc;
	int tc_index = -1;
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
			tc_index = i + ( ( tc / tc_list[ i ] <
							   tc_list[ i + 1 ] / tc ) ? 0 : 1 );
			break;
		}

	if ( tc_index >= 0 &&                                   /* value found ? */
		 fabs( tc - tc_list[ tc_index ] ) > tc * 6.0e-2 )   /* error > 6% ? */
	{
		if ( tc > 1.0 )
			eprint( WARN, SET, "%s: Can't set time constant to %.2f s, "
					"using %.2f s instead.\n", DEVICE_NAME, tc,
					tc_list[ tc_index ] );
		else if ( tc > 1.0e-3 )
			eprint( WARN, SET, "%s: Can't set time constant to %.2f ms,"
					" using %.2f ms instead.\n", DEVICE_NAME,
					tc * 1.0e3, tc_list[ tc_index ] * 1.0e3 );
		else
			eprint( WARN, SET, "%s: Can't set time constant to %.0f us,"
					" using %.0f us instead.\n", DEVICE_NAME,
					tc * 1.0e6, tc_list[ tc_index ] * 1.0e6 );
	}
	
	if ( tc_index < 0 )                                  /* not found yet ? */
	{
		if ( tc < tc_list[ 0 ] )
			tc_index = 0;
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


/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/

Var *lockin_tc( Var *v )
{
	int tc_index;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( FLOAT_VAR, er023m.tc_index == UNDEF_TC_INDEX ?
							  ER023M_TEST_TC_INDEX : er023m.tc_index );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
			return vars_push( FLOAT_VAR, er023m_get_tc( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )

	{
		eprint( WARN, SET, "%s: Float value used as time constant index.\n",
				DEVICE_NAME );
		tc_index =  lrnd( v->val.dval );
	}
	else
		tc_index = v->val.lval;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}
	
	if ( tc_index < 0 || tc_index > TC_MAX_INDEX )
	{
		eprint( FATAL, SET, "%s: Invalid time constant index %ld in %s(), "
				"valid range is 0-%ld.\n",
				DEVICE_NAME, tc_index, TC_MAX_INDEX );
		THROW( EXCEPTION )
	}

	if ( ! TEST_RUN )
	{
		er023m.tc_index = tc_index;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_tc( tc_index );
	}
	
	return vars_push( INT_VAR, tc_index );
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
			return vars_push( FLOAT_VAR, er023m_get_phase( ) );
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
			er023m_set_phase( phase );
	}
	
	return vars_push( FLOAT_VAR, phase );
}


/*----------------------------------------------------------------------*/
/* Returns or sets the modulation attenuation of the lock-in amplifier. */
/* If called without arguments the modulation attenuation is returned.  */
/* If called with an argument the attenuation is set to this value.     */
/*----------------------------------------------------------------------*/

Var *lockin_ref_level( Var *v )
{
	int ma;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( INT_VAR, ( double )
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
			return vars_push( FLOAT_VAR, ( double ) er023m_get_ma( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		ma = ( int ) v->val.lval;
	else
		ma = irnd( v->val.dval );

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
				ma > MOD_OFF : "large" : "low", MIN_MOD_ATT, MOD_OFF );
		THROW( EXCEPTION )
	}

	if ( ! TEST_RUN )
	{
		er023m.ma = ma;
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			er023m_set_ma( ma );
			
	}
	
	return vars_push( FLOAT_VAR, ( double ) ma );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static int er023m_get_rg( void )
{
	char *buf[ 30 ];
	long len = 30;

	if ( gpib_write( er023m.device, "RG\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len ] = '\0';
	return T_atoi( buf );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static void er023m_set_rg( int rg_index )
{
	char buf[ 30 ];


	fsc2_assert( rg_index >=0 && rg_index <= RG_MAX_INDEX );

	sprintf( buf, "RG%d\r" rg_index );
	if ( gpib_write( er023m.device, buf, strlen( buf ) ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static int er023m_get_tc( void )
{
	char *buf[ 30 ];
	long len = 30;

	if ( gpib_write( er023m.device, "TC\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len ] = '\0';
	return T_atoi( buf );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static void er023m_set_tc( int tc_index )
{
	char buf[ 30 ];


	fsc2_assert( tc_index >=0 && tc_index <= RG_MAX_INDEX );

	sprintf( buf, "TC%d\r" tc_index );
	if ( gpib_write( er023m.device, buf, strlen( buf ) ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static int er023m_get_ph( void )
{
	char *buf[ 30 ];
	long len = 30;

	if ( gpib_write( er023m.device, "PH\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len ] = '\0';
	return T_atoi( buf );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static void er023m_set_ph( int ph_index )
{
	char buf[ 30 ];


	fsc2_assert( ph_index >= 0 && ph_index <= PH_MAX_INDEX );

	sprintf( buf, "PH%d\r" tc_index );
	if ( gpib_write( er023m.device, buf, strlen( buf ) ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static int er023m_get_ma( void )
{
	char *buf[ 30 ];
	long len = 30;

	if ( gpib_write( er023m.device, "MA\r", 3 ) == FAILURE ||
		 gpib_read( er023m.device, buf, &len ) == FAILURE )
		er023m_failure( );

	buf[ len ] = '\0';
	return T_atoi( buf );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static void er023m_set_ma( int ma )
{
	char buf[ 30 ];


	fsc2_assert( ma >= MIN_MOD_ATT && ma <= MOD_OFF );

	sprintf( buf, "MA%d\r" tc_index );
	if ( gpib_write( er023m.device, buf, strlen( buf ) ) == FAILURE )
		er023m_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static void er023m_failure( void )
{
	eprint( FATAL, UNSET, "%s: Can't access the lock-in amplifier.\n",
			DEVICE_NAME );
	THROW( EXCEPTION )
}
