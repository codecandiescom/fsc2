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
#include "keithley2600a_limits.h"
#include "keithley2600a_ll.h"
#include "keithley2600a_source.h"
#include "keithley2600a_measure.h"


/* Check that a model is defined */

#if    ! defined _2601A && ! defined _2602A \
    && ! defined _2611A && ! defined _2612A \
    && ! defined _2635 && ! defined _2636A
#error "No model has been defined in configuration file."
#endif

/* Sense modes */

#define SENSE_LOCAL     0      /* 2-wire      */
#define SENSE_REMOTE    1      /* 4-wire      */
#define SENSE_CALA      3      /* calibration */


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


/* Delay constants */

#define DELAY_OFF        0.0
#define DELAY_AUTO      -1.0


/* Source settling modes */

#define SETTLE_SMOOTH           0
#define SETTLE_FAST_RANGE       1
#define SETTLE_FAST_POLARITY    2
#define SETTLE_DIRECT_IRANGE    3
#define SETTLE_SMOOTH_100NA     4
#define SETTLE_FAST_ALL       128


/* Channel autozero modes (off/once/auto) */

#define AUTOZERO_OFF     0
#define AUTOZERO_ONCE    1
#define AUTOZERO_AUTO    2


/* Macros for types of measurements */

#define VOLTAGE      0
#define CURRENT      1
#define POWER        2
#define RESISTANCE   3


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

    double offlimiti;

    double delay;
    int    settling;

    bool   sink;
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
Var_T * sourcemeter_sense_mode( Var_T * v );
Var_T * sourcemeter_source_offmode( Var_T * v );
Var_T * sourcemeter_output_state( Var_T * v );
Var_T * sourcemeter_source_mode( Var_T * v );
Var_T * sourcemeter_source_voltage( Var_T * v );
Var_T * sourcemeter_source_current( Var_T * v );
Var_T * sourcemeter_source_voltage_range( Var_T * v );
Var_T * sourcemeter_source_current_range( Var_T * v );
Var_T * sourcemeter_source_voltage_autoranging( Var_T * v );
Var_T * sourcemeter_source_current_autoranging( Var_T * v );
Var_T * sourcemeter_source_voltage_autorange_low_limit( Var_T * v );
Var_T * sourcemeter_source_current_autorange_low_limit( Var_T * v );
Var_T * sourcemeter_compliance_voltage( Var_T * v );
Var_T * sourcemeter_compliance_current( Var_T * v );
Var_T * sourcemeter_source_delay( Var_T * v );
Var_T * sourcemeter_max_off_source_current( Var_T * v );
Var_T * sourcemeter_source_high_capacity( Var_T * v );
Var_T * sourcemeter_source_sink_mode( Var_T * v );
Var_T * sourcemeter_source_settling_mode( Var_T * v );
Var_T * sourcemeter_measure_voltage( Var_T * v );
Var_T * sourcemeter_measure_current( Var_T * v );
Var_T * sourcemeter_measure_power( Var_T * v );
Var_T * sourcemeter_measure_resistance( Var_T * v );


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
