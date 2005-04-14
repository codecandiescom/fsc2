/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2005 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "hp8647a.conf"


#define MIN_FREQ        2.5e5            /* 250 kHz  */
#define MAX_FREQ        1.0e9            /* 1000 MHz */
#define MIN_MIN_ATTEN   3.0              /* really the minimum attenuation */
#define MAX_ATTEN      -136.0            /* -136 db  */
#define ATT_RESOLUTION  0.1

#define NUM_MOD_TYPES   4
#define MOD_TYPE_FM     0
#define MOD_TYPE_AM     1
#define MOD_TYPE_PHASE  2
#define MOD_TYPE_OFF    3

#define NUM_MOD_SOURCES 4
#define MOD_SOURCE_AC   0
#define MOD_SOURCE_DC   1
#define MOD_SOURCE_1k   2
#define MOD_SOURCE_400  3

#define MAX_FM_AMPL     1.0e5
#define MAX_AM_AMPL     1.0e2
#define MAX_PHASE_AMPL  10.0


#define HP8647A_TEST_FREQ   1.4e6  /*  14 MHz */
#define HP8647A_TEST_ATTEN    -20  /* -20 dB  */


typedef struct Att_Table_Entry Att_Table_Entry_T;
typedef struct HP8647A HP8647A_T;


struct Att_Table_Entry {
	double freq;
	double att;
};


struct HP8647A {
	int device;

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
	Att_Table_Entry_T *att_table;
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
	double mod_ampl[ NUM_MOD_TYPES ];
	bool mod_ampl_is_set[ NUM_MOD_TYPES ];
};


extern HP8647A_T hp8647a;
extern const char *mod_types[ ];
extern const char *mod_sources[ ];


/* Declaration of exported functions */

int hp8647a_init_hook( void );
int hp8647a_test_hook( void );
int hp8647a_exp_hook( void );
int hp8647a_end_of_exp_hook( void );
void hp8647a_exit_hook( void );


Var_T *synthesizer_name( Var_T *v );
Var_T *synthesizer_state( Var_T *v );
Var_T *synthesizer_frequency( Var_T *v );
Var_T *synthesizer_step_frequency( Var_T *v );
Var_T *synthesizer_attenuation( Var_T *v );
Var_T *synthesizer_minimum_attenuation( Var_T *v );
Var_T *synthesizer_sweep_up( Var_T *v );
Var_T *synthesizer_sweep_down( Var_T *v );
Var_T *synthesizer_reset_frequency( Var_T *v );
Var_T *synthesizer_use_table( Var_T *v );
Var_T *synthesizer_attenuation( Var_T *v );
Var_T *synthesizer_att_ref_freq( Var_T *v );
Var_T *synthesizer_modulation( Var_T *v );
Var_T *synthesizer_mod_ampl( Var_T *v );
Var_T *synthesizer_mod_type( Var_T *v );
Var_T *synthesizer_mod_source( Var_T *v );
Var_T *synthesizer_command( Var_T *v );


/* Functions defined in "hp8647a_util.c" */

void hp8647a_read_table( FILE *fp );
FILE *hp8647a_find_table( char **name );
FILE *hp8647a_open_table( char *name );
double hp8647a_get_att_from_table( double freq );
double hp8647a_get_att( double freq );
int hp8647a_set_mod_param( Var_T *v, double *dres, int *ires );


/* Functions defined in "hp8647a_lexer.l" */

void hp8647a_read_table( FILE *fp );


/* Functions defined in "hp8647a_gpib.c" */

bool hp8647a_init( const char *name );
void hp8647a_finished( void );
bool hp8647a_set_output_state( bool state );
bool hp8647a_get_output_state( void );
double hp8647a_set_frequency( double freq );
double hp8647a_get_frequency( void );
double hp8647a_set_attenuation( double att );
double hp8647a_get_attenuation( void );
int hp8647a_set_mod_type( int type );
int hp8647a_get_mod_type( void );
int hp8647a_set_mod_source( int type, int source );
int hp8647a_get_mod_source( int type );
double hp8647a_set_mod_ampl( int type, double ampl );
double hp8647a_get_mod_ampl( int type );
bool hp8647a_command( const char *cmd );


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
