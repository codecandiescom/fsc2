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


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "hp8672a.conf"


#define MIN_FREQ        2.0e9            /* 2 GHz  */
#define MAX_FREQ        1.8e10           /* 18 GHz */
#define MIN_MIN_ATTEN   10.0             /* really the minimum attenuation */
#define MAX_ATTEN      -120.0            /* -120 db  */
#define ATT_RESOLUTION  1

#define NUM_MOD_TYPES   3
#define MOD_TYPE_FM     0
#define MOD_TYPE_AM     1
#define MOD_TYPE_OFF    2

#define MAX_FM_AMPL     1.0e7             /* 10 MHz */
#define MAX_AM_AMPL     1.0e2             /* 100 % */

#define FM_AMPL_SETTINGS 6


/* Bits of the status byte */

#define HP8672A_CRYSTAL_OVEN_COLD     0x80
#define HP8672A_REQUEST_SERVICE       0x40
#define HP8672A_OUT_OF_RANGE          0x20
#define HP8672A_RF_OFF                0x10
#define HP8672A_NOT_PHASE_LOCKED      0x08
#define HP8672A_LEV_UNCAL             0x04
#define HP8672A_FM_OVERMOD            0x02
#define HP8672A_10dbm_OVERRANGE       0x01


/* Declaration of exported functions */

int hp8672a_init_hook( void );
int hp8672a_test_hook( void );
int hp8672a_exp_hook( void );
int hp8672a_end_of_exp_hook( void );
void hp8672a_exit_hook( void );


Var *synthesizer_name( Var *v );
Var *synthesizer_state( Var *v );
Var *synthesizer_frequency( Var *v );
Var *synthesizer_step_frequency( Var *v );
Var *synthesizer_attenuation( Var *v );
Var *synthesizer_minimum_attenuation( Var *v );
Var *synthesizer_sweep_up( Var *v );
Var *synthesizer_sweep_down( Var *v );
Var *synthesizer_reset_frequency( Var *v );
Var *synthesizer_use_table( Var *v );
Var *synthesizer_attenuation( Var *v );
Var *synthesizer_att_ref_freq( Var *v );
Var *synthesizer_modulation( Var *v );
Var *synthesizer_mod_ampl( Var *v );
Var *synthesizer_mod_type( Var *v );
Var *synthesizer_mod_source( Var *v );


typedef struct {
	double freq;
	double att;
} ATT_TABLE_ENTRY;


typedef struct
{
	int device;

	double freq;
	bool freq_is_set;
	double step_freq;
	bool step_freq_is_set;
	double start_freq;
	bool start_freq_is_set;
	double attenuation;
	bool attenuation_is_set;

	bool state;                     /* RF is on or off */
	bool is_10db;                   /* 10 db extra output option */

	char *table_file;               /* name of attenuation table file */
	bool use_table;
	ATT_TABLE_ENTRY *att_table;
	long att_table_len;
	double min_table_freq;
	double max_table_freq;
	double min_attenuation;
	double att_ref_freq;
	double att_at_ref_freq;
	double real_attenuation;        /* might differ from attenuation due to
									   use of table */
	int mod_type;
	bool mod_type_is_set;
	int mod_ampl[ NUM_MOD_TYPES ];
	bool mod_ampl_is_set[ NUM_MOD_TYPES ];

} HP8672A;



#if defined( HP8672A_MAIN )

HP8672A hp8672a;
bool HP8672A_INIT = UNSET;
const char *mod_types[ ] =   { "FM", "AM", "OFF" };
double fm_ampl[ ] = { 3.0e4, 1.0e5, 3.0e5, 1.0e6, 3.0e6, 1.0e7 };
double am_ampl[ ] = { 30.0, 100.0 };

#else

extern HP8672A hp8672a;
extern bool HP8672A_INIT;
extern const char *mod_types[ ];
extern double fm_ampl[ ];
extern double am_ampl[ ];

#endif



/* functions defined in "hp8672a_util.c" */

void hp8672a_read_table( FILE *fp );
FILE *hp8672a_find_table( char **name );
FILE *hp8672a_open_table( char *name );
double hp8672a_get_att_from_table( double freq );
double hp8672a_get_att( double freq );
int hp8672a_set_mod_param( Var *v, double *dres, int *ires );
int hp8672_mod_ampl_check( double ampl );


/* functions defined in "hp8672a_lexer.flex" */

void hp8672a_read_table( FILE *fp );


/* functions defined in "hp8672a_gpib.c" */

bool hp8672a_init( const char *name );
void hp8672a_finished( void );
bool hp8672a_set_output_state( bool state );
bool hp8672a_get_output_state( void );
double hp8672a_set_frequency( double freq );
double hp8672a_set_attenuation( double att );
int hp8672a_set_modulation( void );


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
