/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
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


#if ! defined KEITHLEY2600A_H_
#define KEITHLEY2600A_H_


#include "fsc2_module.h"
#include "keithley2600a.conf"

/* Check that a model is defined */

#if    ! defined _2601A && ! defined _2602A \
    && ! defined _2611A && ! defined _2612A \
    && ! defined _2635 && ! defined _2636A
#error "No model has been defined in configuration file."
#endif

#include "keithley2600a_limits.h"


#define SENSE_LOCAL     0
#define SENSE_REMOTE    1
#define SENSE_CALA      3


/* Outout off modes */

#define OUTPUT_NORMAL    0
#define OUTPUT_HIGH_Z    1
#define OUTPUT_ZERO      2


/* Channel source output states (on/off) */

#define OUTPUT_OFF       0
#define OUTPUT_ON        1

/* Channel source modes (current/voltage) */

#define OUTPUT_DCAMPS    0
#define OUTPUT_DCVOLTS   1

/* Channel autorange states (on/off) */

#define AUTORANGE_OFF    0
#define AUTORANGE_ON     1


/* Channel autozero modes (off/once/auto) */

#define AUTOZERO_OFF     0
#define AUTOZERO_ONCE    1
#define AUTOZERO_AUTO    2


typedef struct
{
    bool   output;
    int    offmode;
    int    func;
    bool   highc;

    bool   autorangev;
    bool   autorangei;

    double rangev;
    double rangei;

    double lowrangev;
    double lowrangei;

    double levelv;
    double leveli;

    double limitv;
    double limiti;
} Source_T;


typedef struct
{
    bool   autorangev;
    bool   autorangei;

    double rangev;
    double rangei;

    double lowrangev;
    double lowrangei;

    int    autozero;
} Measure_T;
    

typedef struct
{
    bool      is_open;
    bool      comm_failed;
    bool      keep_on_at_end[ NUM_CHANNELS ];

    int       sense[ NUM_CHANNELS ];

    Source_T  source[ NUM_CHANNELS ];
    Measure_T measure[ NUM_CHANNELS ];
} Keithley2600A_T;


extern Keithley2600A_T * k26;


/* Hook functions */

int keithley2600a_init_hook( void );
int keithley2600a_test_hook( void );
int keithley2600a_exp_hook( void );
int keithley2600a_end_of_exp_hook( void );


/* EDL functions */

Var_T * sourcemeter_name( Var_T * v );
Var_T * sourcemeter_keep_on_at_end( Var_T * v );
Var_T * sourcemeter_output_state( Var_T * v );
Var_T * sourcemeter_source_mode( Var_T * v );
Var_T * sourcemeter_source_voltage( Var_T * v );
Var_T * sourcemeter_source_current( Var_T * v );
Var_T * sourcemeter_source_voltage_range( Var_T * v );
Var_T * sourcemeter_source_current_range( Var_T * v );
Var_T * sourcemeter_high_capacity( Var_T * v );


/* Internal functions from keithley2600a_ll.c */

bool keithley2600a_open( void );
bool keithley2600a_close( void );
bool keithley2600a_cmd( const char * cmd );
bool keithley2600a_talk( const char * cmd,
                         char       * reply,
                         size_t       length );

void keithley2600a_get_state( void );
void keithley2600a_reset( void );
void keithley2600a_show_errors( void );

bool keithley2600a_line_to_bool( const char * line );
int keithley2600a_line_to_int( const char * line );
double keithley2600a_line_to_double( const char * line );
void keithley2600a_bad_data( void );


/* Internal functions from keithley2600a_source.c */

int keithley2600a_get_sense( unsigned int ch );
int keithley2600a_set_sense( unsigned int ch,
                             int          sense );

int keithley2600a_get_source_offmode( unsigned int ch );
int keithley2600a_set_source_offmode( unsigned int ch,
                                      int          source_offmode );

bool keithley2600a_get_source_output( unsigned int ch );
bool keithley2600a_set_source_output( unsigned int ch,
                                      bool         source_output );

bool keithley2600a_get_source_highc( unsigned int ch );
int keithley2600a_set_source_highc( unsigned int ch,
                                    bool         source_highc );

int keithley2600a_get_source_func( unsigned int ch );
int keithley2600a_set_source_func( unsigned int ch,
                                   int          source_func );

bool keithley2600a_get_source_autorangev( unsigned int ch );
bool keithley2600a_set_source_autorangev( unsigned int ch,
                                          bool         autorange );

bool keithley2600a_get_source_autorangei( unsigned int ch );
bool keithley2600a_set_source_autorangei( unsigned int ch,
                                          bool         autorange );

double keithley2600a_get_source_rangev( unsigned int ch );
double keithley2600a_set_source_rangev( unsigned int ch,
                                        double       range );

double keithley2600a_get_source_rangei( unsigned int ch );
double keithley2600a_set_source_rangei( unsigned int ch,
                                        double       range );


double keithley2600a_get_source_lowrangev( unsigned int ch );
double keithley2600a_set_source_lowrangev( unsigned int ch,
                                           double       lowrange );

double keithley2600a_get_source_lowrangei( unsigned int ch );
double keithley2600a_set_source_lowrangei( unsigned int ch,
                                           double       lowrange );

double keithley2600a_get_source_levelv( unsigned int ch );
double keithley2600a_set_source_levelv( unsigned int ch,
                                        double       volts );

double keithley2600a_get_source_leveli( unsigned int ch );
double keithley2600a_set_source_leveli( unsigned int ch,
                                        double       amps );

double keithley2600a_get_source_limitv( unsigned int ch );
double keithley2600a_set_source_limitv( unsigned int ch,
                                        double       volts );

double keithley2600a_get_source_limiti( unsigned int ch );
double keithley2600a_set_source_limiti( unsigned int ch,
                                        double       amps );

bool keithley2600a_get_compliance( unsigned int ch );


/* Internal functions from keithley_measure.c */

bool keithley2600a_get_measure_autorangev( unsigned int ch );
bool keithley2600a_set_measure_autorangev( unsigned int ch,
                                           bool         autorange );

bool keithley2600a_get_measure_autorangei( unsigned int ch );
bool keithley2600a_set_measure_autorangei( unsigned int ch,
                                           bool         autorange );

int keithley2600a_get_measure_autozero( unsigned int ch );
int keithley2600a_set_measure_autozero( unsigned int ch,
                                        int          autozero );

double keithley2600a_get_measure_lowrangev( unsigned int ch );
double keithley2600a_set_measure_lowrangev( unsigned int ch,
                                            double       lowrange );
double keithley2600a_get_measure_lowrangei( unsigned int ch );
double keithley2600a_set_measure_lowrangei( unsigned int ch,
                                            double       lowrange );

double keithley2600a_get_measure_rangev( unsigned int ch );
double keithley2600a_set_measure_rangev( unsigned int ch,
                                         double       range );
double keithley2600a_get_measure_rangei( unsigned int ch );
double keithley2600a_set_measure_rangei( unsigned int ch,
                                         double       range );


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
