/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once
#if ! defined RS_SML01_HEADER
#define RS_SML01_HEADER


#include "fsc2_module.h"
#include "gpib.h"


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

#define RS_SML01_TEST_RF_FREQ             1.4e7            /* 14 MHz */
#define RS_SML01_TEST_MOD_TYPE            MOD_TYPE_FM
#define RS_SML01_TEST_MOD_SOURCE          MOD_SOURCE_INT
#define RS_SML01_TEST_MOD_FREQ            1.0e5            /* 100 kHz */
#define RS_SML01_TEST_MOD_AMPL            2.5e4            /* 25 kHz */

#define RS_SML01_MAX_TABLE_ENTRIES           10
#define RS_SML01_MAX_TABLE_NAME_LENGTH        7

#define RS_SML01_LEAVE_UCOR_UNCHANGED        -2

#if defined WITH_PULSE_MODULATION

#define SLOPE_FALL      UNSET
#define SLOPE_RAISE     SET

#define MIN_PULSE_WIDTH                   2.0e-8           /* 20 ns */
#define MAX_PULSE_WIDTH                   1.3              /* 1.3 s */
#define MIN_PULSE_DELAY                   2.0e-8           /* 20 ns */
#define MAX_PULSE_DELAY                   1.3              /* 1.3 s */

#define MIN_DOUBLE_PULSE_DELAY            6.0e-8           /* 60 ns */
#define MAX_DOUBLE_PULSE_DELAY            1.3              /* 1.3 s */

#define RS_SML01_TEST_PULSE_MODE_STATE    UNSET
#define RS_SML01_TEST_PULSE_TRIG_SLOPE    SLOPE_RAISE
#define RS_SML01_TEST_PULSE_WIDTH         1.0e-6           /* 1 us */
#define RS_SML01_TEST_PULSE_DELAY         2.0e-8           /* 20 ns */
#define RS_SML01_TEST_ALC_STATE           SET

#define RS_SML01_TEST_DOUBLE_PULSE_MODE   UNSET
#define RS_SML01_TEST_DOUBLE_PULSE_DELAY  1.0e-5           /* 10 us */

#endif /* WITH_PULSE_MODULATION */

struct MOD_RANGES {
    double upper_limit_freq;
    double upper_limit;
};

extern struct MOD_RANGES fm_mod_ranges[ ], pm_mod_ranges[ ];
extern size_t num_fm_mod_ranges, num_pm_mod_ranges;

#define MIN_INT_MOD_FREQ     0.1    /* 0.1 Hz */
#define MAX_INT_MOD_FREQ     1.0e6  /* 1 MHz */


typedef struct Att_Table_Entry Att_Table_Entry_T;
typedef struct RS_SML01 RS_SML01_T;


struct Att_Table_Entry {
    double freq;
    double att;
};


typedef struct {
    char   ** names;
    size_t    cnt;
} User_Corrs;


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

    bool triggered_sweep_is_initialized;
    bool sweep_state;

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
    double mod_freq[ NUM_MOD_TYPES ];
    bool mod_freq_is_set[ NUM_MOD_TYPES ];
    double mod_ampl[ NUM_MOD_TYPES ];
    bool mod_ampl_is_set[ NUM_MOD_TYPES ];

    double freq_change_delay;

    bool alc_state;
    bool alc_state_is_set;

    User_Corrs corrs_req;
    User_Corrs corrs_avail;
    bool corrs_is_set;
    ssize_t corrs_active;

#if defined WITH_PULSE_MODULATION
    bool pulse_mode_state;            /* pulse mode on/off */
    bool pulse_mode_state_is_set;
    bool double_pulse_mode;
    bool double_pulse_mode_is_set;
    bool pulse_trig_slope;
    bool pulse_trig_slope_is_set;
    double pulse_width;
    bool pulse_width_is_set;
    double pulse_delay;
    bool pulse_delay_is_set;
    double double_pulse_delay;
    bool double_pulse_delay_is_set;
#endif /* WITH_PULSE_MODULATION */
};


extern RS_SML01_T rs_sml01;


/* declaration of exported functions */

int rs_sml01_init_hook(       void );
int rs_sml01_test_hook(       void );
int rs_sml01_exp_hook(        void );
int rs_sml01_end_of_exp_hook( void );
void rs_sml01_exit_hook(      void );


Var_T * synthesizer_name(                    Var_T * /* v */ );
Var_T * synthesizer_state(                   Var_T * /* v */ );
Var_T * synthesizer_frequency(               Var_T * /* v */ );
Var_T * synthesizer_step_frequency(          Var_T * /* v */ );
Var_T * synthesizer_triggered_sweep_setup(   Var_T * /* v */ );
Var_T * synthesizer_triggered_sweep_state(   Var_T * /* v */ );
Var_T * synthesizer_attenuation(             Var_T * /* v */ );
Var_T * synthesizer_minimum_attenuation(     Var_T * /* v */ );
Var_T * synthesizer_automatic_level_control( Var_T * /* v */ );
Var_T * synthesizer_user_level_correction(   Var_T * /* v */ );
Var_T * synthesizer_sweep_up(                Var_T * /* v */ );
Var_T * synthesizer_sweep_down(              Var_T * /* v */ );
Var_T * synthesizer_reset_frequency(         Var_T * /* v */ );
Var_T * synthesizer_use_table(               Var_T * /* v */ );
Var_T * synthesizer_att_ref_freq(            Var_T * /* v */ );
Var_T * synthesizer_modulation(              Var_T * /* v */ );
Var_T * synthesizer_mod_freq(                Var_T * /* v */ );
Var_T * synthesizer_mod_ampl(                Var_T * /* v */ );
Var_T * synthesizer_mod_type(                Var_T * /* v */ );
Var_T * synthesizer_mod_source(              Var_T * /* v */ );
Var_T * synthesizer_freq_change_delay(       Var_T * /* v */ );
Var_T * synthesizer_command(                 Var_T * /* v */ );

#if defined WITH_PULSE_MODULATION
Var_T * synthesizer_pulse_state(           Var_T * /* v */ );
Var_T * synthesizer_pulse_trigger_slope(   Var_T * /* v */ );
Var_T * synthesizer_pulse_width(           Var_T * /* v */ );
Var_T * synthesizer_pulse_delay(           Var_T * /* v */ );
Var_T * synthesizer_double_pulse_mode(     Var_T * /* v */ );
Var_T * synthesizer_double_pulse_delay(    Var_T * /* v */ );
#endif /* WITH_PULSE_MODULATION */


/* functions defined in "rs_sml01_util.c" */

FILE * rs_sml01_find_table( char ** /* name */ );

FILE * rs_sml01_open_table( char * /* name */ );

double rs_sml01_get_att_from_table( double /* freq */ );

double rs_sml01_get_att( double /* freq */ );

unsigned int rs_sml01_get_mod_param( Var_T ** /* v    */,
                                     double * /* dres */,
                                     int *    /* ires */  );

void rs_sml01_check_mod_ampl( double /* freq */ );

#if defined WITH_PULSE_MODULATION
char * rs_sml01_pretty_print( double /* t */ );
#endif

/* functions defined in "rs_sml01_lexer.l" */

void rs_sml01_read_table( FILE * /* fp */ );


/* functions defined in "rs_sml01_gpib.c" */

bool rs_sml01_init( const char * /* name */ );

void rs_sml01_finished( void );

bool rs_sml01_set_output_state( bool /* state */ );

bool rs_sml01_get_output_state( void );

double rs_sml01_set_frequency( double /* freq */ );

double rs_sml01_get_frequency( void );

void rs_sml01_triggered_sweep_setup( double /* start_freq */,
                                     double /* end_freq   */,
                                     double /* step_freq  */ );

void rs_sml01_triggered_sweep_state( bool /* state */ );

double rs_sml01_set_attenuation( double /* att */ );

double rs_sml01_get_attenuation( void );

bool rs_sml01_set_automatic_level_control( bool /* state */ );

bool rs_sml01_get_automatic_level_control( void );

size_t rs_sml01_check_ucor_avail_name( const char * /* name */ );

ssize_t rs_sml01_check_ucor_req_name( const char * /* name */ );

bool rs_sml01_get_ucor( void );

void rs_sml01_set_ucor( ssize_t /* index */ );

int rs_sml01_set_mod_type( int /* type */ );

int rs_sml01_get_mod_type( void );

int rs_sml01_set_mod_source( int    /* type   */,
                             int    /* source */,
                             double /* freq   */  );

int rs_sml01_get_mod_source( int      /* type */,
                             double * /* freq */  );

double rs_sml01_set_mod_ampl( int    /* type */,
                              double /* ampl */  );

double rs_sml01_get_mod_ampl( int /* type */ );

bool rs_sml01_command( const char * /* cmd */ );

#if defined WITH_PULSE_MODULATION
void rs_sml01_set_pulse_state( bool /* state */ );

void rs_sml01_set_pulse_trig_slope( bool /* state */ );

void rs_sml01_set_pulse_width( double /* width */ );

void rs_sml01_set_pulse_delay( double /* delay */ );

void rs_sml01_set_double_pulse_mode( bool /* state */ );

void rs_sml01_set_double_pulse_delay( double /* delay */ );
#endif /* WITH_PULSE_MODULATION */

void rs_sml01_setup_triggered_frequency_sweep( double /* step */ );


#endif


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
