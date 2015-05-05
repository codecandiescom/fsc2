#include "keithley2600a.h"


static double max_source_volts_due_to_compliance( unsigned int ch );
static double max_source_amps_due_to_compliance( unsigned int ch );
static double max_compliance_volts_limit( unsigned int ch );
static double max_compliance_amps_limit( unsigned int ch );
static double basic_min_source_rangev( void );
static double basic_max_source_rangev( void );
static double basic_min_source_rangei( void );
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
 * voltage ranges are idential except for the current ranges for
 * the 2635A and 2636A)
 *..................................................................*/

#if defined _2601A || defined _2602A
static double Source_Ranges_V[ ] = { 0.1,
									 1.0,
									 6.0,
									 40.0 };
#define Measure_Ranges_V Source_Ranges_V

static double Source_Ranges_I[ ] = { 1.0e-7,
									 1.0e-6,
									 1.0e-5,
									 1.0e-4,
									 1.0e-3,
									 1.0e-2,
									 1.0e-1,
									 1.0,
									 3.0 };

#define Measure_Ranges_I Source_Ranges_I

#elif defined _26011A || defined _2612A
static double Source_Ranges_V[ ] = { 0.2,
									 2.0,
									 20,
									 200.0 };

#define Measure_Ranges_V Source_Ranges_V

static double Source_Ranges_I[ ] = { 1.0e-7,
									 1.0e-6,
									 1.0e-5,
									 1.0e-4,
									 1.0e-3,
									 1.0e-2,
									 1.0e-1,
									 1.0,
									 1.5,
									 10.0 };

#define Measure_Ranges_I Source_Ranges_U

#else                                   /* 2635A and 2636A */
static double Source_Ranges_V[ ] = { 0.2,
									 2.0,
									 20,
									 200.0 };

#define Measure_Ranges_V Source_Ranges_V

static double Source_Ranges_I[ ] = { 1.0e-9,
									 1.0e-8,
									 1.0e-7,
									 1.0e-6,
									 1.0e-5,
									 1.0e-4,
									 1.0e-3,
									 1.0e-2,
									 1.0e-1,
									 1.0,
									 1.5 };

static double Measure_Ranges_I[ ] = { 1.0e-10,
									  1.0e-9,
									  1.0e-8,
									  1.0e-7,
									  1.0e-6,
									  1.0e-5,
									  1.0e-4,
									  1.0e-3,
									  1.0e-2,
									  1.0e-1,
									  1.0,
									  1.5 };
#endif


/*..................................................................*
 * Maximum and minimum voltage (compliance) limits (upper limits
 * may also depend on range and compliance current setting)
 *..................................................................*/

#if defined _2601A || defined _2611A
#define MIN_SOURCE_LIMITV   1.0e-2
#define MAX_SOURCE_LIMITV   40
#else
#define MIN_SOURCE_LIMITV   2.0e-2
#define MAX_SOURCE_LIMITV   200
#endif


/*..................................................................*
 * Maximum and minimum current (compliance) limits (upper limits
 * may also depend on range and compliance voltage setting)
 *..................................................................*/

#if defined _2601A || defined _2611A || defined _2611A || defined _2612A
#define MIN_SOURCE_LIMITI   1.0e-8
#define MAX_SOURCE_LIMITI   3
#else
#define MIN_SOURCE_LIMITI   1.0e-10
#define MAX_SOURCE_LIMITI   1.5
#endif


/*..................................................................*
 * The maximum compliance voltages and currents when source
 * autoranging is off depend on the current output range setting.
 *..................................................................*/

typedef struct {
    double range;     /* range currently set */
    double limit;     /* maximum limit allowed for that range */
} Max_Limit;


/* Maximum compliance voltage limits in dependence on range setting for
   currrent. 'base' is the limit to be set and  */

#if defined _2601A || defined _2602A
static Max_Limit Max_Compliance_Limits_V[ ] = { {  1, 40 },
                                                {  3,  6 } };
#else
static Max_Limit Max_Compliance_Limits_V[ ] = { { 0.1, 200 },
                                                { 1.5,  20 } };
#endif

/* Maximum current limits in dependence on range setting for voltage */

#if defined _2601A || defined _2602A
static Max_Limit Max_Compliance_Limits_I[ ] = { {  6, 3 },
                                                { 40, 1 } };
#else
static Max_Limit Max_Compliance_Limits_I[ ] = { { 20,  1.5 },
                                                { 200, 0.1 } };
#endif


/*..................................................................*
 * Minimum settings for source current limit, range and low range and
 * minimumn measurement current low limit when high capacity mode is on
 *..................................................................*/

#define MIN_SOURCE_LIMITI_HIGHC     1.0e-6  /* 1 uA */
#define MIN_SOURCE_RANGEI_HIGHC     1.0e-6  /* 1 uA */
#define MIN_SOURCE_LOWRANGEI_HIGHC  1.0e-6  /* 1 uA */
#define MIN_MEASURE_LOWRANGEI_HIGHC 1.0e-6  /* 1 uA */




/*---------------------------------------------------------------*
 * Returns the highest possible voltage for the current setting
 * of the current compliance limit
 *---------------------------------------------------------------*/

static
double
max_source_volts_due_to_compliance( unsigned int ch )
{
	int i;
	int num_levels = ( int ) (   sizeof  Max_Compliance_Limits_I
				    		   / sizeof *Max_Compliance_Limits_I );
	for ( i = 0; i < num_levels; i++ )
		if ( k26->source[ ch ].limiti < Max_Compliance_Limits_I[ i ].range )
			return Max_Compliance_Limits_I[ i ].limit;

	fsc2_assert( 1 == 0 );
	return -1;
}


/*---------------------------------------------------------------*
 * Returns the highest value the compliance voltage can be set
 * to under the current circumstances.
 *---------------------------------------------------------------*/

static
double
max_compliance_volts_limit( unsigned int ch )
{
	double amps;
	int i;
	int num_levels = ( int ) (   sizeof  Max_Compliance_Limits_I
							   / sizeof *Max_Compliance_Limits_I );

	fsc2_assert( ch < NUM_CHANNELS );

	/* If autoranging is on the set current level is what's relevant,
	   otherwise the range setting */

	amps = k26->source[ ch ].autorangei ?
		   k26->source[ ch ].leveli : k26->source[ ch ].rangei;

	for ( i = 0; i < num_levels; i++ )
		if ( amps < Max_Compliance_Limits_I[ i ].range )
			return Max_Compliance_Limits_I[ i ].limit;

	fsc2_assert( 1 == 0 );
	return -1;
}


/*---------------------------------------------------------------*
 * Returns the highest value the compliance current can be set
 * to under the current circumstances.
 *---------------------------------------------------------------*/

static
double
max_compliance_amps_limit( unsigned int ch )
{
	double volts;
	int i;
	int num_levels = ( int ) (   sizeof  Max_Compliance_Limits_V
							   / sizeof *Max_Compliance_Limits_V );

	fsc2_assert( ch < NUM_CHANNELS );

	/* If autoranging is on the set voltage level is what's relevant,
	   otherwise the range setting */

	volts = k26->source[ ch ].autorangev ?
		    k26->source[ ch ].levelv : k26->source[ ch ].rangev;

	for ( i = 0; i < num_levels; i++ )
		if ( volts < Max_Compliance_Limits_V[ i ].range )
			return Max_Compliance_Limits_V[ i ].limit;

	fsc2_assert( 1 == 0 );
	return -1;
}


/*---------------------------------------------------------------*
 * Returns the highest possible current for the current setting
 * of the voltage compliance limit
 *---------------------------------------------------------------*/

static
double
max_source_amps_due_to_compliance( unsigned int ch )
{
	int i;
	int num_levels = ( int ) (   sizeof  Max_Compliance_Limits_V
				    		   / sizeof *Max_Compliance_Limits_V );

	for ( i = 0; i < num_levels; i++ )
		if ( k26->source[ ch ].limitv < Max_Compliance_Limits_V[ i ].range )
			return Max_Compliance_Limits_V[ i ].limit;

	fsc2_assert( 1 == 0 );
	return -1;
}


/*---------------------------------------------------------------*
 * Returns the nearest voltage range for the requested range or a
 * negative value if the value is too large
 *---------------------------------------------------------------*/

double
keithley2600a_best_source_rangev( double range )
{
    int i;
    int num_ranges = ( int ) (   sizeof  Source_Ranges_V
							   / sizeof *Source_Ranges_V );

    fsc2_assert( range > 0 );

    if ( range == 0 )
        return Source_Ranges_V[ 0 ];

    for ( i = 1; i < num_ranges; i++ )
        if ( range <= Source_Ranges_V[ i ] )
            return Source_Ranges_V[ i ];

    return -1;
}
        

/*---------------------------------------------------------------*
 * Returns the nearest current range for the requested range or a
 * negative value if the value is too large.
 *---------------------------------------------------------------*/

double
keithley2600a_best_source_rangei( double range )
{
    int i;
    int num_ranges = ( int ) (   sizeof Source_Ranges_I
							   / sizeof *Source_Ranges_I );

    fsc2_assert( range > 0 );

    if ( range == 0 )
        return Source_Ranges_I[ 0 ];

    for ( i = 1; i < num_ranges; i++ )
        if ( range <= Source_Ranges_I[ i ] )
            return Source_Ranges_I[ i ];

    return -1;
}


/*---------------------------------------------------------------*
 * Returns the maximum source voltage that can be set under the
 * current conditions
 *---------------------------------------------------------------*/

double
keithley2600a_max_source_levelv( unsigned int ch )
{
	double max_volts = MAX_SOURCE_LEVELV;

	fsc2_assert( ch < NUM_CHANNELS );

	/* If output is off or we're in current sourcing mode any value up to
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

	/* If output is off or we're in voltage sourcing mode any value up to
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
keithley2600a_check_source_levelv( double   volts,
								   unsigned int ch )
{
    return fabs( volts ) <= keithley2600a_max_source_levelv( ch );
}


/*---------------------------------------------------------------*
 * Tests if a source current can be set under the current conditions
 *---------------------------------------------------------------*/

bool
keithley2600a_check_source_leveli( double   amps,
								   unsigned int ch )
{
    return fabs( amps ) <= keithley2600a_max_source_leveli( ch );
}


/*---------------------------------------------------------------*
 * Returns the minimum voltage range that cam be set at all
 *---------------------------------------------------------------*/

static
double
basic_min_source_rangev( void )
{
	return Source_Ranges_V[ 0 ];
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
		return basic_min_source_rangev( );

	/* Otherwise we're limited by the output voltage set for the channel,
	   the range must not be smaller */

	return keithley2600a_best_source_rangev( k26->source[ ch ].levelv / 1.01 );
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
	   can be set, si return the maximum one */

	if (    ! k26->source[ ch ].output
		 || k26->source[ ch ].func == OUTPUT_DCAMPS )
		return basic_max_source_rangev( );

	/* Otherwise the maximum range is only limited by the current
	   compliance limit for the channel */

	return max_source_volts_due_to_compliance( ch );
}


/*---------------------------------------------------------------*
 * Returns the minimum current range that can be set at all
 *---------------------------------------------------------------*/

static
double
basic_min_source_rangei( void )
{
	return Source_Ranges_I[ 0 ];
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
		return basic_min_source_rangei( );

	/* Otherwise we're limited by the output current set for the channel,
	   the range must not be smaller  */

	return keithley2600a_best_source_rangei( k26->source[ ch ].leveli / 1.01 );
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
 * Tests if a source voltage range can be set under the current
 * conditions
 *---------------------------------------------------------------*/

bool
keithley2600a_check_source_rangev( double       range,
                                   unsigned int ch )
{
	return    range >= keithley2600a_min_source_rangev( ch )
		   && range <= keithley2600a_max_source_rangev( ch );
}


/*---------------------------------------------------------------*
 * Tests if a source current range can be set under the current
 * conditions
 *---------------------------------------------------------------*/

bool
keithley2600a_check_source_rangei( double       range,
                                   unsigned int ch )
{
	return    range >= keithley2600a_min_source_rangei( ch )
		   && range <= keithley2600a_max_source_rangei( ch );
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
keithley2600a_check_source_limitv( double       limit,
								   unsigned int ch )
{
	return    limit >= keithley2600a_min_source_limitv( ch )
		   && limit <= keithley2600a_max_source_limitv( ch );
}


/*---------------------------------------------------------------*
 * Returns if a current compliance limit can be set under the
 * current circumstances
 *---------------------------------------------------------------*/

bool
keithley2600a_check_source_limiti( double       limit,
								   unsigned int ch )
{
	return    limit >= keithley2600a_min_source_limiti( ch )
		   && limit <= keithley2600a_max_source_limiti( ch );
}


/*---------------------------------------------------------------*
 * Returns the minimum value for the source lowrange voltage setting
 *---------------------------------------------------------------*/

double
keithley_min_source_lowrangev( unsigned int ch  UNUSED_ARG )
{
	return Source_Ranges_V[ 0 ];
}


/*---------------------------------------------------------------*
 * Retirns the minimum value for the source lowrange current setting
 *---------------------------------------------------------------*/

double
keithley_min_source_lowrangei( unsigned int ch )
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
keithley2600a_check_source_lowrangev( double      lowrange,
									  unsigned int ch )
{
	return lowrange >= keithley_min_source_lowrangev( ch );
}


/*---------------------------------------------------------------*
 * Returns if a source lowrange current setting is ok under the
 * current circumstances
 *---------------------------------------------------------------*/

bool
keithley2600a_check_source_lowrangei( double      lowrange,
									  unsigned int ch )
{
	return lowrange >= keithley_min_source_lowrangei( ch );
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
keithley2600a_check_measure_lowrangev( double      lowrange,
									  unsigned int ch )
{
	return lowrange >= keithley2600a_min_measure_lowrangev( ch );
}


/*---------------------------------------------------------------*
 * Returns if a measure lowrange current setting is ok under current
 * circumstances
 *---------------------------------------------------------------*/

bool
keithley2600a_check_measure_lowrangei( double      lowrange,
									   unsigned int ch )
{
	return lowrange >= keithley2600a_min_measure_lowrangei( ch );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
