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


/* Here values are defined that get returned by the driver in the test run
   when the lock-in can't be accessed - these values must really be
   reasonable ! */

#define ER023M_TEST_DATA          0.0
#define ER023M_TEST_RG_INDEX      30        /* receiver gain 2.0e5 */
#define ER023M_TEST_TC_INDEX      15        /* time constant ~ 300 ms */
#define ER023M_TEST_PHASE         0
#define ER023M_TEST_MF_INDEX      1         /* 50 kHz */
#define ER023M_TEST_MOD_ATT       40
#define ER023M_TEST_CT_MULT       102       /* ~ 300 ms */
#define ER023M_TEST_OF            50
#define ER023M_TEST_HARMONIC      0         /* 1st harmonic */
#define ER023M_TEST_RESONATOR     0         /* 1st resonator */


/* Constants and definitions for dealing with the receiver gain */

#define RG_MAX_INDEX          57
#define MIN_RG                2.0e1
#define MAX_RG                1.0e7
#define UNDEF_RG_INDEX        -1

#ifdef ER023M_MAIN
double rg_list[ RG_MAX_INDEX + 1 ];
#else
extern double rg_list[ RG_MAX_INDEX + 1 ];
#endif


/* Constants and definitions for dealing with the time constant -
   we don't allow time constants below 2.56 ms because the minimum
   conversion time we can use is 3.2 ms and having much shorter time
   constants doesn't seem to make sense */

#define TC_MAX_INDEX          19
#define TC_MIN_INDEX          8              /* at least 2.56 us */
#define MAX_TC                5.24288
#define MIN_TC                1.0e-5
#define UNDEF_TC_INDEX        -1

#ifdef ER023M_MAIN
double tc_list[ TC_MAX_INDEX + 1 ];
#else
extern double tc_list[ TC_MAX_INDEX + 1 ];
#endif

/* Constants for dealing with the phase */

#define UNDEF_PHASE           -1


/* Constants and definitions for dealing with the modulation frequency  */

#define MAX_MF_INDEX          6
#define MAX_MOD_FREQ          1.0e5
#define UNDEF_MF_INDEX        -1

#ifdef ER023M_MAIN
double mf_list[ MAX_MF_INDEX + 1 ];
#else
extern double mf_list[ MAX_MF_INDEX + 1 ];
#endif


/* Constants for dealing with the conversion time - we dont allow conversion
   times below 3.2 ms because we need to use "single mode" (where the signal
   channel only sends one ADC value when addressed as talker instead of a
   continuous stream which it does in "continuous mode") */

#define MIN_CT_MULT           10       /* 3.2 ms */
#define MAX_CT_MULT           9999     /* ~ 3.2 s */
#define UNDEF_CT_MULT         -1
#define BASE_CT               3.2e-3   /* minimum conversion time */


/* Constants for dealing with the modulation attenuation */

#define MAX_MOD_ATT           79
#define MIN_MOD_ATT           0
#define MOD_OFF               80
#define UNDEF_MOD_ATT         -1


/* Constants for dealing with the harmonic setting */

#define MAX_HARMONIC          1         /* 2nd harmonic */
#define MIN_HARMONIC          0         /* 1st harmonic */
#define UNDEF_HARMONIC        -1

/* Constants for dealing with the offset setting */

#define MIN_OF                0
#define MAX_OF                99
#define CENTER_OF             50
#define UNDEF_OF              -1

/* Constants for dealing with the resonators */

#define MAX_RESONATOR		  2         /* 2nd resonator*/
#define MIN_RESONATOR		  1         /* 1st resonator */
#define UNDEF_RESONATOR       -1


/* Constants for dealing with mode setting */

#define SINGLE_MODE           0
#define CONTINUOUS_MODE       1


/* Constants for dealing with SRQ setting */

#define SRQ_OFF               0
#define SRQ_ON                1

#define MAX_NB                4          /* maximum number of bytes that get
											send for a ADC data point */


typedef struct
{
	bool is_ph[ 2 ];          /* set if phase is calibrated */
	bool is_ma;               /* set if modulation attenuation is calibrated */

	int pc[ 2 ];              /* phase calibration offset */
	int mc;                   /* modulation attenuation calibration offset */
	double ma_fac;            /* modulation attenuation conversion factor */
} CALIB;


typedef struct {
	int device;

	int rg_index;         /* receiver gain index */
	int tc_index;         /* time constant index */
	int ct_mult;          /* conversion time multiplicator */
	int mf_index;         /* modulation frequency index */
	int phase;            /* modulation phase */
	int ma;               /* modulation attenuation */
	int of;               /* offset */
	int ha;               /* harmonic setting */
	int re;               /* resonator number */

	CALIB calib[ MAX_MF_INDEX + 1 ];

	int nb;               /* number of bytes send from ADC */
	                      /* recheck whenever CT changes */
	double scale_factor;
	int scale_offset;

	unsigned char st;     /* status byte */
	bool st_is_valid;     /* when set use the stored value of the status byte,
							 otherwise fetch from device */

} ER023M;


#ifdef ER023M_MAIN
ER023M er023m;
#else
extern ER023M er023m;
#endif


/* Declaration of exported functions */

int er023m_init_hook( void );
int er023m_exp_hook( void );
int er023m_end_of_exp_hook( void );
void er023m_exit_hook( void );

Var *lockin_name( Var *v );
Var *lockin_get_data( Var *v );
Var *lockin_sensitivity( Var *v );
Var *lockin_time_constant( Var *v );
Var *lockin_phase( Var *v );
Var *lockin_offset( Var *v );
Var *lockin_conversion_time( Var *v );
Var *lockin_ref_freq( Var *v );
Var *lockin_harmonic( Var *v );
Var *lockin_resonator( Var *v );
Var *lockin_is_overload( Var *v );


Var *lockin_rg( Var *v );
Var *lockin_tc( Var *v );
Var *lockin_ma( Var *v );
Var *lockin_ct( Var *v );
Var *lockin_mf( Var *v );


bool er023m_init( const char *name );
unsigned int er023m_get_data( void );
int er023m_get_rg( void );
void er023m_set_rg( int rg_index );
int er023m_get_tc( void );
void er023m_set_tc( int tc_index );
int er023m_get_ph( void );
void er023m_set_ph( int ph_index );
int er023m_get_ma( void );
void er023m_set_ma( int ma );
int er023m_get_of( void );
void er023m_set_of( int of );
int er023m_get_ct( void );
void er023m_set_ct( int ct_mult );
int er023m_get_mf( void );
void er023m_set_mf( int mf_index ) ;
int er023m_get_ha( void );
void er023m_set_ha( int ha );
int er023m_get_re( void );
void er023m_set_re( int re );
int er023m_nb( void );
void er023m_srq( int on_off );
unsigned char er023m_st( void );
void er023m_failure( void );
