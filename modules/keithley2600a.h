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


/* Define the maximum number of points to be returned from a sweep - the
   value isn't documented and is derived from the observation that the
   device may stop sending anymore data after having sent 10200 bytes (10
   VXI-11 send buffers of 1020 bytes). Now, each point it sends requires
   up to 14 bytes (including a leading space and a trailing comma) when
   uing ASCII snd 4 when using binaray, and the value is the result of
   dividing 10200 by 14 (or 10197 by 4). For simultaneous measrements
   of voltage and current the two data points get sent, which reduces
   the number of points in a sweep to halve that value. */

#if ! defined BINARY_TRANSFER
#define MAX_SWEEP_RESULT_POINTS  728
#else
#define MAX_SWEEP_RESULT_POINTS  2549
#endif


/* Check that a model is defined */

#if    ! defined _2601A && ! defined _2602A \
    && ! defined _2611A && ! defined _2612A \
    && ! defined _2635A && ! defined _2636A
#error "No supported model defined in configuration file."
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

/* Filter types */

#define FILTER_MOVING_AVG  0
#define FILTER_REPEAT_AVG  1
#define FILTER_MEDIAN      2

#define MAX_FILTER_COUNT   100

/* Macros for types of measurements */

#define VOLTAGE              0
#define CURRENT              1
#define POWER                2
#define RESISTANCE           3
#define VOLTAGE_AND_CURRENT  4


/* Minimum and maximum factors (to be multiplied by the reciprocal of the
   line frequency) for reading aperture/integration time */

#define MIN_INTEGRATION_FACTOR 0.001
#define MAZ_INTEGRATION_FACTOR 25


/* Contact measurement speeds */

#define CONTACT_FAST    0
#define CONTACT_MEDIUM  1
#define CONTACT_SLOW    2

#define MIN_CONTACT_CURRENT_LIMIT  1.0e-3   /* 1 mA */



typedef struct
{
    bool   output;
    int    offmode;
    bool   func;
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
    double level;
    bool   enabled;
} Rel_T;


typedef struct
{
    int  type;
    int  count;
    bool enabled;
} Filter_T;


typedef struct
{
    double threshold;
    int    speed;
} Contact_T;


typedef struct
{
    bool   autorangev;
    bool   autorangei;

    double rangev;
    double rangei;

    double lowrangev;
    double lowrangei;

    int    autozero;

    int    count;

    double time;

    double delay;

    Rel_T  relv;
    Rel_T  reli;

    Filter_T filter;

    long extra_delay;            /* Number of  us to aditionally wait (once)
                                    after changing measure time */
} Measure_T;
    

typedef struct
{
    bool      is_open;
    bool      comm_failed;
    bool      keep_on_at_end[ NUM_CHANNELS ];

    int       sense[ NUM_CHANNELS ];

    double linefreq;

    Source_T  source[ NUM_CHANNELS ];
    Measure_T measure[ NUM_CHANNELS ];
    Contact_T contact[ NUM_CHANNELS ];

    bool lin_sweeps_prepared;
    bool list_sweeps_prepared;
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
Var_T * sourcemeter_test_compliance( Var_T * v );
Var_T * sourcemeter_source_delay( Var_T * v );
Var_T * sourcemeter_source_max_off_current( Var_T * v );
Var_T * sourcemeter_source_high_capacity( Var_T * v );
Var_T * sourcemeter_source_sink_mode( Var_T * v );
Var_T * sourcemeter_source_settling_mode( Var_T * v );
Var_T * sourcemeter_measure_voltage( Var_T * v );
Var_T * sourcemeter_measure_current( Var_T * v );
Var_T * sourcemeter_measure_power( Var_T * v );
Var_T * sourcemeter_measure_resistance( Var_T * v );
Var_T * sourcemeter_measure_voltage_and_current( Var_T * v );
Var_T * sourcemeter_measure_voltage_range( Var_T * v );
Var_T * sourcemeter_measure_current_range( Var_T * v );
Var_T * sourcemeter_measure_voltage_autoranging( Var_T * v );
Var_T * sourcemeter_measure_current_autoranging( Var_T * v );
Var_T * sourcemeter_measure_voltage_autorange_low_limit( Var_T * v );
Var_T * sourcemeter_measure_current_autorange_low_limit( Var_T * v );
Var_T * sourcemeter_measure_time( Var_T * v );
Var_T * sourcemeter_measure_voltage_offset( Var_T * v );
Var_T * sourcemeter_measure_current_offset( Var_T * v );
Var_T * sourcemeter_measure_delay( Var_T * v );
Var_T * sourcemeter_measure_filter_type( Var_T * v );
Var_T * sourcemeter_measure_filter_count( Var_T * v );
Var_T * sourcemeter_sweep_voltage_measure_voltage( Var_T * v );
Var_T * sourcemeter_sweep_voltage_measure_current( Var_T * v );
Var_T * sourcemeter_sweep_voltage_measure_power( Var_T * v );
Var_T * sourcemeter_sweep_voltage_measure_resistance( Var_T * v );
Var_T * sourcemeter_sweep_voltage_measure_voltage_and_current( Var_T * v );
Var_T * sourcemeter_sweep_current_measure_voltage( Var_T * v );
Var_T * sourcemeter_sweep_current_measure_current( Var_T * v );
Var_T * sourcemeter_sweep_current_measure_power( Var_T * v );
Var_T * sourcemeter_sweep_current_measure_resistance( Var_T * v );
Var_T * sourcemeter_sweep_current_measure_voltage_and_current( Var_T * v );
Var_T * sourcemeter_list_sweep_voltage_measure_voltage( Var_T * v );
Var_T * sourcemeter_list_sweep_voltage_measure_current( Var_T * v );
Var_T * sourcemeter_list_sweep_voltage_measure_power( Var_T * v );
Var_T * sourcemeter_list_sweep_voltage_measure_resistance( Var_T * v );
Var_T * sourcemeter_list_sweep_voltage_measure_voltage_and_current( Var_T * v );
Var_T * sourcemeter_list_sweep_current_measure_voltage( Var_T * v );
Var_T * sourcemeter_list_sweep_current_measure_current( Var_T * v );
Var_T * sourcemeter_list_sweep_current_measure_power( Var_T * v );
Var_T * sourcemeter_list_sweep_current_measure_resistance( Var_T * v );
Var_T * sourcemeter_list_sweep_current_measure_voltage_and_current( Var_T * v );
Var_T * sourcemeter_contact_check( Var_T * v );
Var_T * sourcemeter_contact_resistance( Var_T * v );
Var_T * sourcemeter_contact_threshold( Var_T * v );
Var_T * sourcemeter_contact_speed( Var_T * v );


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
