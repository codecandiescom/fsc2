/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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

#include "rs_sml01.conf"


#define MIN_FREQ        9.0e3            /* 9 kHz  */
#define MAX_FREQ        1.1e9            /* 1.1 GHz */
#define MIN_MIN_ATTEN   13.0             /* really the minimum attenuation */
#define MAX_ATTEN      -140.0            /* -140 dBm */
#define ATT_RESOLUTION  0.1              /* 0.1 dBm */

#define NUM_MOD_TYPES   4
#define MOD_TYPE_FM     0
#define MOD_TYPE_AM     1
#define MOD_TYPE_PM     2
#define MOD_TYPE_OFF    3

#define NUM_MOD_SOURCES 3
#define MOD_SOURCE_AC   0
#define MOD_SOURCE_DC   1
#define MOD_SOURCE_INT  2

#define MAX_AM_AMPL     1.0e2


#define RS_SML01_TEST_RF_FREQ     1.4e7            /* 14 MHz */
#define RS_SML01_TEST_MOD_TYPE    MOD_TYPE_FM
#define RS_SML01_TEST_MOD_SOURCE  MOD_SOURCE_INT
#define RS_SML01_TEST_MOD_FREQ    1.0e5            /* 100 kHz */
#define RS_SML01_TEST_MOD_AMPL    2.5e4            /* 25 kHz */


struct MOD_RANGES {
	double upper_limit_freq;
	double upper_limit;
};

extern struct MOD_RANGES fm_mod_ranges[ ], pm_mod_ranges[ ];
extern size_t num_fm_mod_ranges, num_pm_mod_ranges;

#define MIN_INT_MOD_FREQ     0.1    /* 0.1 Hz */
#define MAX_INT_MOD_FREQ     1.0e6  /* 1 MHz */


/* declaration of exported functions */

int rs_sml01_init_hook( void );
int rs_sml01_test_hook( void );
int rs_sml01_exp_hook( void );
int rs_sml01_end_of_exp_hook( void );
void rs_sml01_exit_hook( void );


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
Var *synthesizer_mod_freq( Var *v );
Var *synthesizer_mod_ampl( Var *v );
Var *synthesizer_mod_type( Var *v );
Var *synthesizer_mod_source( Var *v );
Var *synthesizer_command( Var *v );


typedef struct ATT_TABLE_ENTRY ATT_TABLE_ENTRY;
typedef struct RS_SML01 RS_SML01;


struct ATT_TABLE_ENTRY {
	double freq;
	double att;
};


struct RS_SML01 {
	double freq;
	bool freq_is_set;
	double step_freq;
	bool step_freq_is_set;
	double start_freq;
	bool start_freq_is_set;
	double attenuation;
	bool attenuation_is_set;

	bool state;

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
	int mod_source[ NUM_MOD_TYPES ];
	bool mod_source_is_set[ NUM_MOD_TYPES ];
	double mod_freq[ NUM_MOD_TYPES ];
	bool mod_freq_is_set[ NUM_MOD_TYPES ];
	double mod_ampl[ NUM_MOD_TYPES ];
	bool mod_ampl_is_set[ NUM_MOD_TYPES ];
};


extern RS_SML01 rs_sml01;
extern const char *mod_types[ ];
extern const char *mod_sources[ ];


/* functions defined in "rs_sml01_util.c" */

void rs_sml01_read_table( FILE *fp );
FILE *rs_sml01_find_table( char **name );
FILE *rs_sml01_open_table( char *name );
double rs_sml01_get_att_from_table( double freq );
double rs_sml01_get_att( double freq );
unsigned int rs_sml01_get_mod_param( Var **v, double *dres, int *ires );
void rs_sml01_check_mod_ampl( double freq );


/* functions defined in "rs_sml01_lexer.l" */

void rs_sml01_read_table( FILE *fp );


/* functions defined in "rs_sml01_gpib.c" */

bool rs_sml01_init( const char *name );
void rs_sml01_finished( void );
bool rs_sml01_set_output_state( bool state );
bool rs_sml01_get_output_state( void );
double rs_sml01_set_frequency( double freq );
double rs_sml01_get_frequency( void );
double rs_sml01_set_attenuation( double att );
double rs_sml01_get_attenuation( void );
int rs_sml01_set_mod_type( int type );
int rs_sml01_get_mod_type( void );
int rs_sml01_set_mod_source( int type, int source, double freq );
int rs_sml01_get_mod_source( int type, double *freq );
double rs_sml01_set_mod_ampl( int type, double ampl );
double rs_sml01_get_mod_ampl( int type );
bool rs_sml01_command( const char *cmd );


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
