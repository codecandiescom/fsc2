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


#include "keithley2600a.h"


/* Local functions */

static double max_source_volts_due_to_compliance( unsigned int ch );
static double max_source_amps_due_to_compliance( unsigned int ch );
static double max_compliance_volts_limit( unsigned int ch );
static double max_compliance_amps_limit( unsigned int ch );
static double basic_max_source_rangev( void );
static double basic_max_source_rangei( void );
static double basic_max_source_limitv( void );
static double basic_max_source_limiti( void );


/*..................................................................*
 * Maximum source voltages and currents
 *..................................................................*/

#if defined _2601A || defined _2602A
#define MAX_SOURCE_LEVELV  40.4
#define MAX_SOURCE_LEVELI  3.03
#else
#define MAX_SOURCE_LEVELV  202.0
#define MAX_SOURCE_LEVELI  1.515
#endif


/*..................................................................*
 * Source and measurement voltage and current ranges (source and
 * voltage ranges are identical except for the current ranges of
 * the 2635A and 2636A)
 *..................................................................*/

#if defined _2601A || defined _2602A

static double Source_Ranges_V[ ] = {  0.1,      /* 100 mV */
									  1.0,      /*   1  V */
									  6.0,      /*   6  V */
									 40.0 };    /*  40  V */
#define Measure_Ranges_V Source_Ranges_V

static double Source_Ranges_I[ ] = { 1.0e-7,    /* 100 nA */
									 1.0e-6,    /*   1 uA */
									 1.0e-5,    /*  10 uA */
									 1.0e-4,    /* 100 uA */
									 1.0e-3,    /*   1 mA */
									 1.0e-2,    /*  10 mA */
									 1.0e-1,    /* 100 mA */
									 1.0,       /*   1  A */
                                     3.0 };     /*   3  A */

#define Measure_Ranges_I Source_Ranges_I

#elif defined _2611A || defined _2612A

static double Source_Ranges_V[ ] = {   0.2,     /* 200 mV */
									   2.0,     /*   2  V */
									  20.0,     /*  20  V */
									 200.0 };   /* 200  V */

#define Measure_Ranges_V Source_Ranges_V

static double Source_Ranges_I[ ] = {  1.0e-7,   /* 100 nA */
									  1.0e-6,   /*   1 uA */
									  1.0e-5,   /*  10 uA */
									  1.0e-4,   /* 100 uA */
									  1.0e-3,   /*   1 mA */
									  1.0e-2,   /*  10 mA */
									  1.0e-1,   /* 100 mA */
									  1.0,      /*   1  A */
									  1.5,      /*  1.5 A */
									 10.0 };    /*  10  A */

#define Measure_Ranges_I Source_Ranges_I

#elif defined _2635A || defined _2636A

static double Source_Ranges_V[ ] = {   0.2,     /* 200 mV */
									   2.0,     /*   2  V */
									  20.0,     /*  20  V */
									 200.0 };   /* 200  V */

#define Measure_Ranges_V Source_Ranges_V

static double Source_Ranges_I[ ] = { 1.0e-9,    /*   1 nA */
									 1.0e-8,    /*  10 nA */
									 1.0e-7,    /* 100 nA */
									 1.0e-6,    /*   1 uA */
									 1.0e-5,    /*  10 uA */
									 1.0e-4,    /* 100 uA */
									 1.0e-3,    /*   1 mA */
									 1.0e-2,    /*  10 mA */
									 1.0e-1,    /* 100 mA */
									 1.0,       /*   1  A */
									 1.5 };     /*  1.5 A */

static double Measure_Ranges_I[ ] = { 1.0e-10,  /* 100 pA */
									  1.0e-9,   /*   1 nA */
									  1.0e-8,   /*  10 nA */
									  1.0e-7,   /* 100 nA */
									  1.0e-6,   /*   1 uA */
									  1.0e-5,   /*  10 uA */
									  1.0e-4,   /* 100 uA */
									  1.0e-3,   /*   1 mA */
									  1.0e-2,   /*  10 mA */
									  1.0e-1,   /* 100 mA */
									  1.0,      /*   1  A */
									  1.5 };    /*  1.5 A */
#endif

static size_t Num_Source_Ranges_V =   sizeof  Source_Ranges_V
                                    / sizeof *Source_Ranges_V;
static size_t Num_Measure_Ranges_V =   sizeof  Measure_Ranges_V
                                     / sizeof *Measure_Ranges_V;

static size_t Num_Source_Ranges_I =   sizeof  Source_Ranges_I
                                    / sizeof *Source_Ranges_I;
static size_t Num_Measure_Ranges_I =   sizeof  Measure_Ranges_I
                                     / sizeof *Measure_Ranges_I;


/*..................................................................*
 * Minimum and maximum voltage (compliance) limits
 *..................................................................*/

#if defined _2601A || defined _2611A
#define MIN_SOURCE_LIMITV   1.0e-2
#define MAX_SOURCE_LIMITV   40
#else
#define MIN_SOURCE_LIMITV   2.0e-2
#define MAX_SOURCE_LIMITV   200
#endif


/*..................................................................*
 * Minimum and maximum current (compliance) limits
 *..................................................................*/

#if defined _2601A || defined _2611A || defined _2611A || defined _2612A

#define MIN_SOURCE_LIMITI   1.0e-8
#define MAX_SOURCE_LIMITI   3

#elif defined _2635A || defined _2636A

#define MIN_SOURCE_LIMITI   1.0e-10
#define MAX_SOURCE_LIMITI   1.5

#endif


/*..................................................................*
 * The maximum compliance voltages and currents that can be set
 * depend on the source current/voltage level or range settings
 * (and vice versa) and change at certain points, which are
 * defined via the 'Max_Limit' structure.
 *..................................................................*/

typedef struct {
    double range;     /* level or range currently set */
    double limit;     /* maximum limit for that level or range */
} Limit_Point;


#if defined _2601A || defined _2602A

static Limit_Point Limits_I_to_V[ ] = { {  1.0, 40.0 },
                                        {  3.0,  6.0 } };

static Limit_Point Limits_V_to_I[ ] = { {  6.0, 3.0 },
                                        { 40.0, 1.0 } };

#elif defined _2611A || defined _2612A || defined _2635A || defined _2636A

static Limit_Point Limits_I_to_V[ ] = { {  0.1, 200.0 },
                                        {  1.5,  20.0 } };

static Limit_Point Limits_V_to_I[ ] = { {  20.0,  1.5 },
                                        { 200.0,  0.1 } };

#endif

static size_t Num_Limits_V_to_I = sizeof Limits_V_to_I / sizeof *Limits_V_to_I;
static size_t Num_Limits_I_to_V = sizeof Limits_I_to_V / sizeof *Limits_I_to_V;


/*..................................................................*
 * Minimum settings for source current limit, range and low range and
 * minimumn measurement current low range in high capacity mode
 *..................................................................*/

#define MIN_SOURCE_LIMITI_HIGHC     1.0e-6     /* 1 uA */
#define MIN_SOURCE_RANGEI_HIGHC     1.0e-6     /* 1 uA */
#define MIN_SOURCE_LOWRANGEI_HIGHC  1.0e-6     /* 1 uA */
#define MIN_MEASURE_LOWRANGEI_HIGHC 1.0e-6     /* 1 uA */



/*---------------------------------------------------------------*
 * Returns the highest possible source voltage dependening on
 * the the current compliance limit setting
 *---------------------------------------------------------------*/

static
double
max_source_volts_due_to_compliance( unsigned int ch )
{
	size_t i;

	for ( i = 0; i < Num_Limits_I_to_V; i++ )
		if ( k26->source[ ch ].limiti <= Limits_I_to_V[ i ].range )
			return Limits_I_to_V[ i ].limit;

	fsc2_assert( 1 == 0 );
	return -1;
}


/*---------------------------------------------------------------*
 * Returns the highest possible source current depending on the
 * the voltage compliance limit setting
 *---------------------------------------------------------------*/

static
double
max_source_amps_due_to_compliance( unsigned int ch )
{
	size_t i;

	for ( i = 0; i < Num_Limits_V_to_I; i++ )
		if ( k26->source[ ch ].limitv <= Limits_V_to_I[ i ].range )
			return Limits_V_to_I[ i ].limit;

	fsc2_assert( 1 == 0 );
	return -1;
}


/*---------------------------------------------------------------*
 * Returns the highest possible compliance voltage depending on
 * the source current level or range (if autoranging is off) setting
 *---------------------------------------------------------------*/

static
double
max_compliance_volts_limit( unsigned int ch )
{
	double amps;
	size_t i;

	/* If autoranging is on the set current level is what's relevant,
	   otherwise the range setting */

	amps = k26->source[ ch ].autorangei ?
		   k26->source[ ch ].leveli : k26->source[ ch ].rangei;

	for ( i = 0; i < Num_Limits_I_to_V; i++ )
		if ( amps <= Limits_I_to_V[ i ].range )
			return Limits_I_to_V[ i ].limit;

	fsc2_assert( 1 == 0 );
	return -1;
}


/*---------------------------------------------------------------*
 * Returns the highest possible compliance current depending on the
 * source voltage level or range (if autoranging is off) setting
 *---------------------------------------------------------------*/

static
double
max_compliance_amps_limit( unsigned int ch )
{
	double volts;
	size_t i;

	fsc2_assert( ch < NUM_CHANNELS );

	/* If autoranging is on the set voltage level is what's relevant,
	   otherwise the range setting */

	volts = k26->source[ ch ].autorangev ?
		    k26->source[ ch ].levelv : k26->source[ ch ].rangev;

	for ( i = 0; i < Num_Limits_V_to_I; i++ )
		if ( volts <= Limits_V_to_I[ i ].range )
			return Limits_V_to_I[ i ].limit;

	fsc2_assert( 1 == 0 );
	return -1;
}


/*---------------------------------------------------------------*
 * Returns the nearest voltage range for the requested range or level.
 * A negative value is returned if the value is too large.
 *---------------------------------------------------------------*/

double
keithley2600a_best_source_rangev( unsigned int ch  UNUSED_ARG,
                                  double       volts )
{
    size_t i;
    size_t num_ranges = sizeof  Source_Ranges_V / sizeof *Source_Ranges_V;

    volts = fabs( volts );

    for ( i = 0; i < num_ranges; i++ )
        if ( volts <= Source_Ranges_V[ i ] )
            return Source_Ranges_V[ i ];

    return -1;
}
        

/*---------------------------------------------------------------*
 * Returns the nearest current range for the requested range or level.
 * A negative value is returned if the value is too large.
 *---------------------------------------------------------------*/

double
keithley2600a_best_source_rangei( unsigned int ch  UNUSED_ARG,
                                  double       amps )
{
    size_t i;
    size_t num_ranges = sizeof Source_Ranges_I / sizeof *Source_Ranges_I;

    amps = fabs( amps );

    /* Find the lowest ramge the value fits in, but keep in mind that there's
       a raised minimum setting when the channel is in high capacity mode */

    for ( i = 0; i < num_ranges; i++ )
        if ( amps <= Source_Ranges_I[ i ] )
        {
            if ( k26->source[ ch ].highc )
                return d_max( Source_Ranges_I[ i ], MIN_SOURCE_RANGEI_HIGHC );
            return Source_Ranges_I[ i ];
        }

    return -1;
}


/*---------------------------------------------------------------*
 * Returns the maximum source voltage that can be set under the
 * current circumstances
 *---------------------------------------------------------------*/

double
keithley2600a_max_source_levelv( unsigned int ch )
{
	double max_volts = MAX_SOURCE_LEVELV;

	fsc2_assert( ch < NUM_CHANNELS );

	/* If output is off or we're in current sourcing mode any level up to
	   the highest possible voltage can be set */

	if (    ! k26->source[ ch ].output
		 || k26->source[ ch ].func == OUTPUT_DCAMPS )
		return max_volts;

	/* Otherwise we're limitated due to a current compliance value */

	max_volts = max_source_volts_due_to_compliance( ch );

	/* And if autranging is off we also could be further limited by the
	   currently set range. */

	if ( k26->source[ ch ].autorangev )
		max_volts = d_min( max_volts, 1.01 * k26->source[ ch ].rangev );

    return max_volts;
}


/*---------------------------------------------------------------*
 * Returns the maximum source curren that can be set under the
 * current conditions
 *---------------------------------------------------------------*/

double
keithley2600a_max_source_leveli( unsigned int ch )
{
	double max_amps = MAX_SOURCE_LEVELI;

	fsc2_assert( ch < NUM_CHANNELS );

	/* If output is off or we're in voltage sourcing mode any level up to
	   the highest possible current can be set */

	if (    ! k26->source[ ch ].output
		 || k26->source[ ch ].func == OUTPUT_DCVOLTS )
		return max_amps;

	/* Otherwise we are limitated due to a voltage compliance value */

	max_amps = max_source_amps_due_to_compliance( ch );

	/* And if autranging is off we also could be further limited by the
	   currently set range. */

	if ( k26->source[ ch ].autorangei )
		max_amps = d_min( max_amps, 1.01 * k26->source[ ch ].rangei );

    return max_amps;
}


/*---------------------------------------------------------------*
 * Tests if a source voltage can be set under the current conditions
 *---------------------------------------------------------------*/

bool
keithley2600a_check_source_levelv( unsigned int ch,
								   double       volts )
{
    return fabs( volts ) <= keithley2600a_max_source_levelv( ch );
}


/*---------------------------------------------------------------*
 * Tests if the set source voltage can be applied under the current
 * conditions
 *---------------------------------------------------------------*/

bool
keithley2600a_test_source_levelv( unsigned int ch )
{
    return keithley2600a_check_source_levelv( ch, 
                                              k26->source[ ch ].levelv );
}


/*---------------------------------------------------------------*
 * Tests if a source current can be set under the current conditions
 *---------------------------------------------------------------*/

bool
keithley2600a_check_source_leveli( unsigned int ch,
								   double       amps )
{
    return fabs( amps ) <= keithley2600a_max_source_leveli( ch );
}


/*---------------------------------------------------------------*
 * Tests if the currently set source current can be applied under
 * the current conditions
 *---------------------------------------------------------------*/

bool
keithley2600a_test_source_leveli( unsigned int ch )
{
    return keithley2600a_check_source_leveli( ch,
                                              k26->source[ ch ].leveli );
}


/*---------------------------------------------------------------*
 * Returns the minimum voltage range that cam be set under the
 * corrent conditions
 *---------------------------------------------------------------*/

double
keithley2600a_min_source_rangev( unsigned int ch )
{
	fsc2_assert( ch < NUM_CHANNELS );

	/* If output is off or we're in current sourcing mode all ranges
	   can be set */

	if (    ! k26->source[ ch ].output
		 || k26->source[ ch ].func == OUTPUT_DCAMPS )
		return Source_Ranges_V[ 0 ];

	/* Otherwise we're limited by the output voltage set for the channel,
	   the range must not be smaller */

	return keithley2600a_best_source_rangev( ch,
                                             k26->source[ ch ].levelv / 1.01 );
}


/*---------------------------------------------------------------*
 * Returns the maximum voltage range that cam be set at all
 *---------------------------------------------------------------*/
static
double
basic_max_source_rangev( void )
{
	return Source_Ranges_V[   sizeof  Source_Ranges_V
							/ sizeof *Source_Ranges_V - 1 ];
}


/*---------------------------------------------------------------*
 * Returns the maximum voltage range that can be set under the
 * corrent conditions (this is dependent on the current compliance
 * setting)
 *---------------------------------------------------------------*/

double
keithley2600a_max_source_rangev( unsigned int ch )
{
	fsc2_assert( ch < NUM_CHANNELS );

	/* If output is off or we're in current sourcing mode all ranges
	   can be set, so return the maximum one */

	if (    ! k26->source[ ch ].output
		 || k26->source[ ch ].func == OUTPUT_DCAMPS )
		return basic_max_source_rangev( );

	/* Otherwise the maximum range is only limited by the current
	   compliance limit for the channel */

	return max_source_volts_due_to_compliance( ch );
}


/*---------------------------------------------------------------*
 * Returns the minimum current range that cam be set under the
 * corrent conditions
 *---------------------------------------------------------------*/

double
keithley2600a_min_source_rangei( unsigned int ch )
{
	/* If output is off or we're in voltage sourcing mode all currenr
	   ranges can be set, so return the smallest one */

	if (    ! k26->source[ ch ].output
		 || k26->source[ ch ].func == OUTPUT_DCVOLTS )
		return Source_Ranges_I[ 0 ];

	/* Otherwise we're limited by the output current set for the channel,
	   the range must not be smaller  */

	return keithley2600a_best_source_rangei( ch,
                                             k26->source[ ch ].leveli / 1.01 );
}


/*---------------------------------------------------------------*
 * Returns the maximum current range that cam be set at all
 *---------------------------------------------------------------*/

static
double
basic_max_source_rangei( void )
{
	return Source_Ranges_I[   sizeof  Source_Ranges_I
							/ sizeof *Source_Ranges_I - 1 ];
}


/*---------------------------------------------------------------*
 * Returns the maximum current range that cam be set under the
 * corrent conditions (this is dependent on the voltage compliance
 * setting)
 *---------------------------------------------------------------*/

double
keithley2600a_max_source_rangei( unsigned int ch )
{
	fsc2_assert( ch < NUM_CHANNELS );

	/* If output is off or we're in voltage sourcing mode all ranges
	   can be set, so return the maximum one */

	if (    ! k26->source[ ch ].output
		 || k26->source[ ch ].func == OUTPUT_DCVOLTS )
		return basic_max_source_rangei( );

	/* Otherwise we're limited only by the voltage comliance value */

	return max_source_amps_due_to_compliance( ch );
}


/*---------------------------------------------------------------*
 * Tests if a source voltage range is ok and can be set under the
 * current conditions
 *---------------------------------------------------------------*/

bool
keithley2600a_check_source_rangev( unsigned int ch,
                                   double       range )
{
    size_t i;
    double min_range = keithley2600a_min_source_rangev( ch );
    double max_range = keithley2600a_max_source_rangev( ch );

    range = fabs( range );

    for ( i = 0; i < Num_Source_Ranges_V; i++ )
        if ( Source_Ranges_V[ i ] == min_range )
            break;

    for ( ; i < Num_Source_Ranges_V && Source_Ranges_V[ i ] <= max_range; i++ )
        if ( range <= Source_Ranges_V[ i ] )
            return OK;

    return FAIL;
}


/*---------------------------------------------------------------*
 * Tests if the currently set source voltage range can be applied
 * under the current conditions
 *---------------------------------------------------------------*/

bool
keithley2600a_test_source_rangev( unsigned int ch )
{
    double range = k26->source[ ch ].rangev;

    if ( k26->source[ ch ].autorangev )
        return OK;

    return    keithley2600a_check_source_rangev( ch, range )
           && 1.01 * range >= fabs( k26->source[ ch ].levelv );
}


/*---------------------------------------------------------------*
 * Tests if a source current range can be set under the current
 * conditions
 *---------------------------------------------------------------*/

bool
keithley2600a_check_source_rangei( unsigned int ch,
                                   double       range )
{
    size_t i;
    double min_range = keithley2600a_min_source_rangei( ch );
    double max_range = keithley2600a_max_source_rangei( ch );

    for ( i = 0; i < Num_Source_Ranges_I; i++ )
        if ( Source_Ranges_I[ i ] == min_range )
            break;

    for ( ; i < Num_Source_Ranges_I && Source_Ranges_I[ i ] <= max_range; i++ )
        if ( range == Source_Ranges_I[ i ] )
            return OK;

    return FAIL;
}


/*---------------------------------------------------------------*
 * Tests if the currently set source current range can be applied
 * under the current conditions
 *---------------------------------------------------------------*/

bool
keithley2600a_test_source_rangei( unsigned int ch )
{
    double range = k26->source[ ch ].rangei;

    if ( k26->source[ ch ].autorangei )
        return OK;

    return    keithley2600a_check_source_rangei( ch, range )
           && 1.01 * range >= fabs( k26->source[ ch ].leveli );
}


/*---------------------------------------------------------------*
 * Returns the maximum possible voltage compliance limit that can
 * be set at all
 *---------------------------------------------------------------*/

static
double
basic_max_source_limitv( void )
{
	return MAX_SOURCE_LIMITV;
}


/*---------------------------------------------------------------*
 * Returns the maximum possible voltage compliance limit under the
 * current conditions.
 *---------------------------------------------------------------*/

double
keithley2600a_max_source_limitv( unsigned int ch )
{
	fsc2_assert( ch < NUM_CHANNELS );

	/*  If output is off or we're in current sourcing mode the highest
		possible value can be set */

	if (    ! k26->source[ ch ].output
		 || k26->source[ ch ].func == OUTPUT_DCAMPS )
		return basic_max_source_limitv( );

	/* Otherwise we're limited by a source voltage or range setting */

	return max_compliance_volts_limit( ch );
}


/*---------------------------------------------------------------*
 * Returns the maximum possible current compliance limit that
 * can be set at all
 *---------------------------------------------------------------*/

static
double
basic_max_source_limiti( void )
{
	return MAX_SOURCE_LIMITI;
}


/*---------------------------------------------------------------*
 * Returns the maximum possible current compliance limit under the
 * current conditions.
 *---------------------------------------------------------------*/

double
keithley2600a_max_source_limiti( unsigned int ch )
{
	fsc2_assert( ch < NUM_CHANNELS );

	/*  If output is off or we're in current sourcing mode the highest
		possible value can be set */

	if (    ! k26->source[ ch ].output
		 || k26->source[ ch ].func == OUTPUT_DCAMPS )
		return basic_max_source_limiti( );

	/* Otherwise we're limited by a source current or range setting */

	return max_compliance_amps_limit( ch );
}


/*---------------------------------------------------------------*
 * Returns the minimum possible voltage compliance limit that can
 * be set at all
 *---------------------------------------------------------------*/

double
keithley2600a_min_source_limitv( unsigned int ch  UNUSED_ARG )
{
	return MIN_SOURCE_LIMITV;
}


/*---------------------------------------------------------------*
 * Returns the minimum possible current compliance limit that can
 * be set at all
 *---------------------------------------------------------------*/

double
keithley2600a_min_source_limiti( unsigned int ch )
{
	fsc2_assert( ch < NUM_CHANNELS );

	if (    ! k26->source[ ch ].output
		 || ! k26->source[ ch ].highc )
		return MIN_SOURCE_LIMITI;

	return MIN_SOURCE_LIMITI_HIGHC;
}


/*---------------------------------------------------------------*
 * Returns if a voltage compliance limit can be set under the
 * current circumstances
 *---------------------------------------------------------------*/

bool
keithley2600a_check_source_limitv( unsigned int ch,
								   double       limit )
{
	return    limit >= keithley2600a_min_source_limitv( ch )
		   && limit <= keithley2600a_max_source_limitv( ch );
}


/*---------------------------------------------------------------*
 * Returns if the currently set voltage compliance limit can be applied
 * under the current circumstances
 *---------------------------------------------------------------*/

bool
keithley2600a_test_source_limitv( unsigned int ch )
{
	return keithley2600a_check_source_limitv( ch,
                                              k26->source[ ch ].limitv );
}


/*---------------------------------------------------------------*
 * Returns if a current compliance limit can be set under the
 * current circumstances
 *---------------------------------------------------------------*/

bool
keithley2600a_check_source_limiti( unsigned int ch,
								   double       limit )
{
	return    limit >= keithley2600a_min_source_limiti( ch )
		   && limit <= keithley2600a_max_source_limiti( ch );
}


/*---------------------------------------------------------------*
 * Returns if the currently set current compliance limit can be
 * applied under the current circumstances
 *---------------------------------------------------------------*/

bool
keithley2600a_test_source_limiti( unsigned int ch )
{
	return keithley2600a_check_source_limiti( ch,
                                              k26->source[ ch ].limiti );
}


/*---------------------------------------------------------------*
 * Returns the minimum value for the source lowrange voltage setting
 *---------------------------------------------------------------*/

double
keithley2600a_min_source_lowrangev( unsigned int ch  UNUSED_ARG )
{
	return Source_Ranges_V[ 0 ];
}


/*---------------------------------------------------------------*
 * Returns the minimum value for the source lowrange current setting
 *---------------------------------------------------------------*/

double
keithley2600a_min_source_lowrangei( unsigned int ch )
{
	if (    ! k26->source[ ch ].output
		 || ! k26->source[ ch ].highc )
		return Source_Ranges_I[ 0 ];

	return MIN_SOURCE_LOWRANGEI_HIGHC;
}


/*---------------------------------------------------------------*
 * Returns if a source lowrange voltage setting is ok under current
 * circumstances
 *---------------------------------------------------------------*/

bool
keithley2600a_check_source_lowrangev( unsigned int ch,
									  double       lowrange )
{
	return    lowrange >= keithley2600a_min_source_lowrangev( ch )
           && lowrange <= keithley2600a_max_source_rangev( ch );
}


/*---------------------------------------------------------------*
 * Returns if a source lowrange current setting is ok under the
 * current circumstances
 *---------------------------------------------------------------*/

bool
keithley2600a_check_source_lowrangei( unsigned int ch,
									  double       lowrange )
{
	return lowrange >= keithley2600a_min_source_lowrangei( ch );
}


/*---------------------------------------------------------------*
 * Returns the minimum value for the measure lowrange voltage setting
 *---------------------------------------------------------------*/

double
keithley2600a_min_measure_lowrangev( unsigned int ch  UNUSED_ARG )
{
	return Measure_Ranges_V[ 0 ];
}


/*---------------------------------------------------------------*
 * Retirns the minimum value for the measure lowrange current setting
 *---------------------------------------------------------------*/

double
keithley2600a_min_measure_lowrangei( unsigned int ch )
{
	if (    ! k26->source[ ch ].output
		 || ! k26->source[ ch ].highc )
		return Measure_Ranges_I[ 0 ];

	return MIN_MEASURE_LOWRANGEI_HIGHC;
}


/*---------------------------------------------------------------*
 * Returns if a lowrange voltage setting is ok under current circumstances
 *---------------------------------------------------------------*/

bool
keithley2600a_check_measure_lowrangev( unsigned int ch,
									   double       lowrange )
{
	return    lowrange >= keithley2600a_min_measure_lowrangev( ch )
           && lowrange <= keithley2600a_max_source_rangei( ch );
}


/*---------------------------------------------------------------*
 * Returns the minimum off limit current - it's not properly documented
 * but seems to be 10% of the lowest source current range setting
 *---------------------------------------------------------------*/

double
keithley2600a_min_source_offlimiti( unsigned int ch  UNUSED_ARG )
{
    return 0.1 * Source_Ranges_I[ 0 ];
}


/*---------------------------------------------------------------*
 * Returns the maximum off limit current - it's not properly documented
 * but seems to be the highest source current range setting
 *---------------------------------------------------------------*/

double
keithley2600a_max_source_offlimiti( unsigned int ch  UNUSED_ARG )
{
    return basic_max_source_rangei( );
}


/*---------------------------------------------------------------*
 * Checks if off limit current s ok
 *---------------------------------------------------------------*/

bool
keithley2600a_check_source_offlimiti( unsigned int ch,
                                      double       offlimiti )
{
    return    offlimiti >= keithley2600a_min_source_offlimiti( ch )
           && offlimiti <= keithley2600a_max_source_offlimiti( ch );
}


/*---------------------------------------------------------------*
 * Returns if a measure lowrange current setting is ok under current
 * circumstances
 *---------------------------------------------------------------*/

bool
keithley2600a_check_measure_lowrangei( unsigned int ch,
									   double       lowrange )
{
	return lowrange >= keithley2600a_min_measure_lowrangei( ch );
}


/*---------------------------------------------------------------*
 * Returns if a measure voltage range setting is ok
 *---------------------------------------------------------------*/

bool
keithley2600a_check_measure_rangev( unsigned int ch  UNUSED_ARG,
                                    double   range )
{
    size_t i;

    for ( i = 0; i < Num_Measure_Ranges_V; i++ )
        if ( range <= Measure_Ranges_V[ i ] )
            return OK;

    return FAIL;
}


/*---------------------------------------------------------------*
 * Returns if a measure voltage range setting is ok
 *---------------------------------------------------------------*/

bool
keithley2600a_check_measure_rangei( unsigned int ch  UNUSED_ARG,
                                    double       range )
{
    size_t i;

    for ( i = 0; i < Num_Measure_Ranges_I; i++ )
        if ( range <= Measure_Ranges_I[ i ] )
            return OK;

    return FAIL;
}


/*---------------------------------------------------------------*
 * Returns if output of channel can be toggled on or off
 *---------------------------------------------------------------*/

bool
keithley2600a_test_toggle_source_output( unsigned int ch )
{
    bool res;

    /* Switching the channel off is always possible */

    if ( k26->source[ ch ].output )
        return true;

    k26->source[ ch ].output = true;

    res =    (    k26->source[ ch ].output == OUTPUT_DCAMPS
               && keithley2600a_test_source_leveli( ch )
               && keithley2600a_test_source_rangei( ch )
               && keithley2600a_test_source_limitv( ch ) )
          || (    k26->source[ ch ].output == OUTPUT_DCVOLTS
               && keithley2600a_test_source_levelv( ch )
               && keithley2600a_test_source_rangev( ch )
               && keithley2600a_test_source_limiti( ch ) );

    k26->source[ ch ].output = false;

    return res;
}


/*---------------------------------------------------------------*
 * Returns if source function can be toggled (i.e from voltage to
 * current source or vice versa)
 *---------------------------------------------------------------*/

bool
keithley2600a_test_toggle_source_func( unsigned int ch )
{
    bool res;

    /* As long as output is off there's no possible problem */

    if ( k26->source[ ch ].output )
        return true;

    k26->source[ ch ].func = ! k26->source[ ch ].func;

    res =    (    k26->source[ ch ].func == OUTPUT_DCAMPS
               && keithley2600a_test_source_leveli( ch )
               && keithley2600a_test_source_rangei( ch )
               && keithley2600a_test_source_limitv( ch ) )
          || (   k26->source[ ch ].func == OUTPUT_DCVOLTS
              && keithley2600a_test_source_levelv( ch )
              && keithley2600a_test_source_rangev( ch )
              && keithley2600a_test_source_limiti( ch ) );

    k26->source[ ch ].func = ! k26->source[ ch ].func;

    return res;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
