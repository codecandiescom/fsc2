#if ! defined KEITHLEY2600A_LIMITS_H_
#define KEITHLEY2600A_LIMITS_H_

#include "keithley2600a.h"



/* Number of of channels */

#if defined _2601A || defined _2611A || defined _2635A
#define NUM_CHANNELS 1
#else
#define NUM_CHANNELS 2
#endif

double keithley2600a_best_source_rangev( double range );
double keithley2600a_best_source_rangei( double range );

double keithley2600a_max_source_levelv( unsigned int ch );
double keithley2600a_max_source_leveli( unsigned int ch );
bool keithley2600a_check_source_levelv( double   volts,
										unsigned int ch );
bool keithley2600a_check_source_leveli( double   amps,
										unsigned int ch );

double keithley2600a_min_source_rangev( unsigned int ch );
double keithley2600a_max_source_rangev( unsigned int ch );
double keithley2600a_min_source_rangei( unsigned int ch );
double keithley2600a_max_source_rangei( unsigned int ch );
bool keithley2600a_check_source_rangev( double       range,
										unsigned int ch );
bool keithley2600a_check_source_rangei( double       range,
										unsigned int ch );

double keithley2600a_max_source_limitv( unsigned int ch );
double keithley2600a_max_source_limiti( unsigned int ch );
double keithley2600a_min_source_limitv( unsigned int ch );
double keithley2600a_min_source_limiti( unsigned int ch );

bool keithley2600a_check_source_limitv( double       limit,
										unsigned int ch );
bool keithley2600a_check_source_limiti( double       limit,
										unsigned int ch);

double keithley_min_source_lowrangev( unsigned int ch );
double keithley_min_source_lowrangei( unsigned int ch );

bool keithley2600a_check_source_lowrangev( double      lowrange,
                                           unsigned int ch );
bool keithley2600a_check_source_lowrangei( double      lowrange,
                                           unsigned int ch );

double keithley2600a_min_measure_lowrangev( unsigned int ch );
double keithley2600a_min_measure_lowrangei( unsigned int ch );
bool keithley2600a_check_measure_lowrangev( double      lowrange,
                                            unsigned int ch );
bool keithley2600a_check_measure_lowrangei( double      lowrange,
                                            unsigned int ch );


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
