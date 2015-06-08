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


#include "fsc2_module.h"
#include "vxi11_user.h"


/* Include configuration information for the device */

#include "rs_smb100a.conf"


#if    ! defined B101 && ! defined B102   \
    && ! defined B103 && ! defined B106   \
    && ! defined B112 && ! defined B112L
#error "Hardware option of device not defined in configuration"
#endif


/* Define the minimum frequency the device can produce */

#if defined B101 || defined B102 || defined B103 || defined B106
#define MIN_FREQ        9.0e3            /* 9 kHz  */
#else                                    /* B112 & B112L */
#define  MIN_FREQ       1.0e5            /* 100 kHz */
#endif


/* Define the maximum frequency the device can produce */

#if defined   B101
#define MAX_FREQ        1.1e9            /* 1.1 GHz */
#elif defined B102
#define MAX_FREQ        2.2e9            /* 2.2 GHz */
#elif defined B103
#define MAX_FREQ        3.2e9            /* 3.2 GHz */
#elif defined B106
#define MAX_FREQ        6.0e9            /* 6.0 GHz */
#else                                    /* B112 & B112L */
#define MAX_FREQ        1.275e10         /* 12.75 GHz */
#endif


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

#define RS_SMB100A_TEST_RF_FREQ             1.4e8            /* 140 MHz */
#define RS_SMB100A_TEST_MOD_TYPE            MOD_TYPE_FM
#define RS_SMB100A_TEST_MOD_SOURCE          MOD_SOURCE_INT
#define RS_SMB100A_TEST_MOD_FREQ            1.0e5            /* 100 kHz */
#define RS_SMB100A_TEST_MOD_AMPL            2.5e4            /* 25 kHz */
#define RS_SMB100A_TEST_INPUT_TRIG_SLOPE    SLOPE_RAISE
#define RS_SMB100A_TEST_INPUT_IMPEDANCE     HIGH_IMPEDANCE

#define SLOPE_FALL      false
#define SLOPE_RAISE     true

#define G600_IMPEDANCE  false
#define HIGH_IMPEDANCE  true

#if defined WITH_PULSE_MODULATION

#define MIN_PULSE_WIDTH                     2.0e-8           /* 20 ns */
#define MAX_PULSE_WIDTH                     1.3              /* 1.3 s */
#define MIN_PULSE_DELAY                     2.0e-8           /* 20 ns */
#define MAX_PULSE_DELAY                     1.3              /* 1.3 s */

#define MIN_DOUBLE_PULSE_DELAY              6.0e-8           /* 60 ns */
#define MAX_DOUBLE_PULSE_DELAY              1.3              /* 1.3 s */

#define RS_SMB100A_TEST_PULSE_MODE_STATE    UNSET
#define RS_SMB100A_TEST_PULSE_WIDTH         1.0e-6           /* 1 us */
#define RS_SMB100A_TEST_PULSE_DELAY         2.0e-8           /* 20 ns */

#define RS_SMB100A_TEST_DOUBLE_PULSE_MODE   UNSET
#define RS_SMB100A_TEST_DOUBLE_PULSE_DELAY  1.0e-5           /* 10 us */

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
typedef struct RS_SMB100A RS_SMB100A_T;


struct Att_Table_Entry {
    double freq;
    double att;
};


struct RS_SMB100A {
	int    device;

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
    double mod_freq[ NUM_MOD_TYPES ];
    bool mod_freq_is_set[ NUM_MOD_TYPES ];
    double mod_ampl[ NUM_MOD_TYPES ];
    bool mod_ampl_is_set[ NUM_MOD_TYPES ];

    double freq_change_delay;

    bool input_trig_slope;
    bool input_trig_slope_is_set;

    bool input_imp;
    bool input_imp_is_set;

#if defined WITH_PULSE_MODULATION
    bool pulse_mode_state;            /* pulse mode on/off */
    bool pulse_mode_state_is_set;
    bool double_pulse_mode;
    bool double_pulse_mode_is_set;
    double pulse_width;
    bool pulse_width_is_set;
    double pulse_delay;
    bool pulse_delay_is_set;
    double double_pulse_delay;
    bool double_pulse_delay_is_set;
#endif /* WITH_PULSE_MODULATION */
};


extern RS_SMB100A_T rs_smb100a;


/* declaration of exported functions */

int rs_smb100a_init_hook(       void );
int rs_smb100a_test_hook(       void );
int rs_smb100a_exp_hook(        void );
int rs_smb100a_end_of_exp_hook( void );
void rs_smb100a_exit_hook(      void );


Var_T * synthesizer_name(                Var_T * /* v */ );
Var_T * synthesizer_state(               Var_T * /* v */ );
Var_T * synthesizer_frequency(           Var_T * /* v */ );
Var_T * synthesizer_step_frequency(      Var_T * /* v */ );
Var_T * synthesizer_attenuation(         Var_T * /* v */ );
Var_T * synthesizer_minimum_attenuation( Var_T * /* v */ );
Var_T * synthesizer_sweep_up(            Var_T * /* v */ );
Var_T * synthesizer_sweep_down(          Var_T * /* v */ );
Var_T * synthesizer_reset_frequency(     Var_T * /* v */ );
Var_T * synthesizer_use_table(           Var_T * /* v */ );
Var_T * synthesizer_att_ref_freq(        Var_T * /* v */ );
Var_T * synthesizer_modulation(          Var_T * /* v */ );
Var_T * synthesizer_mod_freq(            Var_T * /* v */ );
Var_T * synthesizer_mod_ampl(            Var_T * /* v */ );
Var_T * synthesizer_mod_type(            Var_T * /* v */ );
Var_T * synthesizer_mod_source(          Var_T * /* v */ );
Var_T * synthesizer_freq_change_delay(   Var_T * /* v */ );
Var_T * synthesizer_command(             Var_T * /* v */ );
Var_T * synthesizer_input_trigger_slope( Var_T * /* v */ );
Var_T * synthesizer_input_impedance(     Var_T * /* v */ );

#if defined WITH_PULSE_MODULATION
Var_T * synthesizer_pulse_state(         Var_T * /* v */ );
#endif // WITH_PULSE_MODULATION

#if defined WITH_PULSE_GENERATION
Var_T * synthesizer_pulse_width(         Var_T * /* v */ );
Var_T * synthesizer_pulse_delay(         Var_T * /* v */ );
Var_T * synthesizer_double_pulse_mode(   Var_T * /* v */ );
Var_T * synthesizer_double_pulse_delay(  Var_T * /* v */ );
#endif // WITH_PULSE_GENERATION


/* functions defined in "rs_smb100a_util.c" */

FILE * rs_smb100a_find_table( char ** /* name */ );

FILE * rs_smb100a_open_table( char * /* name */ );

double rs_smb100a_get_att_from_table( double /* freq */ );

double rs_smb100a_get_att( double /* freq */ );

unsigned int rs_smb100a_get_mod_param( Var_T ** /* v    */,
                                     double * /* dres */,
                                     int *    /* ires */  );

void rs_smb100a_check_mod_ampl( double /* freq */ );

#if defined WITH_PULSE_MODULATION
char * rs_smb100a_pretty_print( double /* t */ );
#endif

/* functions defined in "rs_smb100a_lexer.l" */

void rs_smb100a_read_table( FILE * /* fp */ );


/* functions defined in "rs_smb100a_gpib.c" */

bool rs_smb100a_init( const char * /* name */ );

void rs_smb100a_finished( void );

bool rs_smb100a_set_output_state( bool /* state */ );

bool rs_smb100a_get_output_state( void );

double rs_smb100a_set_frequency( double /* freq */ );

double rs_smb100a_get_frequency( void );

double rs_smb100a_set_attenuation( double /* att */ );

double rs_smb100a_get_attenuation( void );

int rs_smb100a_set_mod_type( int /* type */ );

int rs_smb100a_get_mod_type( void );

int rs_smb100a_set_mod_source( int    /* type   */,
                               int    /* source */,
                               double /* freq   */  );

int rs_smb100a_get_mod_source( int      /* type */,
                               double * /* freq */  );

double rs_smb100a_set_mod_ampl( int    /* type */,
                                double /* ampl */  );

double rs_smb100a_get_mod_ampl( int /* type */ );

bool rs_smb100a_command( const char * /* cmd */ );

void rs_smb100a_set_input_trig_slope( bool /* state */ );

bool rs_smb100a_get_input_trig_slope( void );

void rs_smb100a_set_input_impedance( bool /* state */ );

bool rs_smb100a_get_input_impedance( void );

#if defined WITH_PULSE_MODULATION
void rs_smb100a_set_pulse_state( bool /* state */ );

void rs_smb100a_set_pulse_width( double /* width */ );

void rs_smb100a_set_pulse_delay( double /* delay */ );

void rs_smb100a_set_double_pulse_mode( bool /* state */ );

void rs_smb100a_set_double_pulse_delay( double /* delay */ );
#endif /* WITH_PULSE_MODULATION */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
