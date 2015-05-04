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


#if    ! defined _2601A && ! defined _2602A \
    && ! defined _2611A && ! defined _2612A \
    && ! defined _2635 && ! defined _2636A
#error "No model has been defined in configuration file."
#endif



/* Maximium of channels */

#if defined _2601A || defined _2611A || defined _2635A
#define NUM_CHANNELS 1
#else
#define NUM_CHANNELS 2
#endif


/* Maximum output values */

#if defined _2601A || defined _2602A
#define MAX_SOURCE_LEVELV  40
#define MAX_SOURCE_LEVELI   3
#else
#define MAX_SOURCE_LEVELV  200
#define MAX_SOURCE_LEVELI  1.5
#endif


/* Minimum limit values (for compliance) - maximum values depend on range */

#if defined _2601A || defined _2602A
#define MIN_LIMITV  1.0e-2
#else
#define MIN_LIMITV  2.0e-2
#endif

#if defined _2601A || defined _2602A || defined _2611A || defined _2612A
#define MIN_LIMITI 1.0e-8
#else
#define MIN_LIMITI 1.0e-10
#endif


typedef struct
{
	double base;
	double limit;
} Max_Limit;


extern Max_Limit keithley2600s_max_limitv[ ];
extern Max_Limit keithley2600s_max_limiti[ ];


#define SENSE_LOCAL     0
#define SENSE_REMOTE    1
#define SENSE_CALA      3


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
	int    offmode[ NUM_CHANNELS ];
    bool   output[ NUM_CHANNELS ];
    bool   highc[ NUM_CHANNELS ];
    int    func[ NUM_CHANNELS ];

    double rangev[ NUM_CHANNELS ];
    double rangei[ NUM_CHANNELS ];

	double levelv[ NUM_CHANNELS ];
	double leveli[ NUM_CHANNELS ];

    double lowrangev[ NUM_CHANNELS ];
    double lowrangei[ NUM_CHANNELS ];

    double limitv[ NUM_CHANNELS ];
    double limiti[ NUM_CHANNELS ];
} Source_T;


typedef struct
{
    bool   autorangev[ NUM_CHANNELS ];
    bool   autorangei[ NUM_CHANNELS ];

    double rangev[ NUM_CHANNELS ];
    double rangei[ NUM_CHANNELS ];

    double lowrangev[ NUM_CHANNELS ];
    double lowrangei[ NUM_CHANNELS ];

    int    autozero[ NUM_CHANNELS ];
} Measure_T;
    

typedef struct
{
    bool      is_open;

	int       sense[ NUM_CHANNELS ];

    Source_T  source;
    Measure_T measure;
} Keithley2600A_T;


extern Keithley2600A_T * k26;


/* Hook functions */

int keithley2600a_test_hook( void );
int keithley2600a_exp_hook( void );
int keithley2600a_end_of_exp_hook( void );


/* EDL functions */

Var_T * sourcemeter_name( Var_T * v );
Var_T * sourcemeter_output( Var_T * v );


/* Internal functions */

void keithley2600a_get_state( void );

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

bool keithley2600a_get_measure_autorangev( unsigned int ch );
bool keithley2600a_set_measure_autorangev( unsigned int ch,
                                           bool         autorange );

bool keithley2600a_get_measure_autorangei( unsigned int ch );
bool keithley2600a_set_measure_autorangei( unsigned int ch,
                                           bool         autorange );

int keithley2600a_get_measure_autozero( unsigned int ch );
int keithley2600a_set_measure_autozero( unsigned int ch,
                                        int          autozero );

bool keithley2600a_get_compliance( unsigned int ch );

double keithley2600a_get_source_levelv( unsigned int ch );
double keithley2600a_set_source_levelv( unsigned int ch,
                                        double       volts );

double keithley2600a_get_source_leveli( unsigned int ch );
double keithley2600a_set_source_leveli( unsigned int ch,
                                        double       amps );

bool keithley2600a_open( void );
bool keithley2600a_close( void );



#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
