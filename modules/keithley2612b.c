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


#include "keithley2612b.h"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

static Var_T * do_sweep( Var_T * v,
                         int     sweep_what,
                         int     masure_what );
static void get_lin_sweep_params( Var_T        * v,
                                  int            sweep_what,
                                  int            measure_what,
                                  unsigned int * ch,
                                  double       * start,
                                  double       * end,
                                  int          * num_points );
static Var_T * do_list_sweep( Var_T * v,
                              int     sweep_what,
                              int     measure_what );
static Var_T * get_list_sweep_params( Var_T        * v,
                                      int            sweep_what,
                                      int            measure_what,
                                      unsigned int * ch );
static void check_sweep_data( const double * data,
                              int            num_points );
static unsigned int get_channel( Var_T ** v );
static void correct_for_highc( unsigned int ch );
static const char * ppc( unsigned int   ch,
                         const char   * prefix );
static void default_settings( void );


static Keithley2600A_T keithley2612b;
static Keithley2600A_T keithley2612b_test;
Keithley2600A_T * k26 = &keithley2612b;


#define TEST_VOLTAGE         1.0e-3
#define TEST_CURRENT         1.0e-6
#define TEST_COMPLIANCE      0L
#define TEST_CONTACT_R_LOW   5.0
#define TEST_CONTACT_R_HIGH  8.0


#if ! defined POWER_LINE_FREQ
#define POWER_LINE_FREQ  50
#endif


/*--------------------------------------------------------------*
 * Init hook
 *--------------------------------------------------------------*/

int
keithley2612b_init_hook( void )
{
    k26 = &keithley2612b;

    k26->is_open = false;
    k26->comm_failed = false;

    for ( unsigned int ch = 0; ch < NUM_CHANNELS; ch++ )
        k26->keep_on_at_end[ ch ] = false;

    return 1;
}


/*--------------------------------------------------------------*
 * Start of test hook
 *--------------------------------------------------------------*/

int
keithley2612b_test_hook( void )
{
    keithley2612b_test = keithley2612b;
    k26 = &keithley2612b_test;

    /* Set up the structures for both channels with the device's default
       settings */

    k26->linefreq = POWER_LINE_FREQ;
    k26->reset_on_start = false;
    default_settings( );
 
    return true;
}


/*--------------------------------------------------------------*
 * Start of experiment hook
 *--------------------------------------------------------------*/

int
keithley2612b_exp_hook( void )
{
    k26 = &keithley2612b;

    if ( ! keithley2612b_open( ) )
        return false;

    TRY
    {
        if ( k26->reset_on_start )
            keithley2612b_reset( );
        else
            keithley2612b_get_state( );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        keithley2612b_close( );
        RETHROW;
    }

    return true;
}
   

/*--------------------------------------------------------------*
 * End of experiment hook
 *--------------------------------------------------------------*/

int
keithley2612b_end_of_exp_hook( void )
{
    /* If there was no communication failure switch off all channels
       unless the user explicitely asked for them to be left on. On
       communication failures we better don't try any further communication,
       so we have to leave the channels on but we warn the user about it */

    if ( ! k26->comm_failed )
    {
        for ( unsigned int ch = 0; ch < NUM_CHANNELS; ch++ )
            if ( k26->source[ ch ].output && ! k26->keep_on_at_end[ ch ] )
                keithley2612b_set_source_output( ch, OUTPUT_OFF );
    }
    else
    {
        for ( unsigned int ch = 0; ch < NUM_CHANNELS; ch++ )
            if ( k26->source[ ch ].output )
            {
                print( WARN, "Please be careful: can't switch off output of "
                       "device!\n" );
                break;
            }
    }

    /* Close connection to the device */

    return keithley2612b_close( );
}


/*--------------------------------------------------------------*
 * Returns the device name
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*--------------------------------------------------------------*
 * Allows to request that one or all (if called with no argument)
 * channels are left in the output on state at the end of the
 * experiment (per default they get switched off automatically).
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_keep_on_at_end( Var_T * v )
{
    unsigned int ch = NUM_CHANNELS;

    if ( v )
        ch = get_channel( &v );

    if ( ch != NUM_CHANNELS )
        k26->keep_on_at_end[ ch ] = true;
    else
        for ( ch = 0; ch < NUM_CHANNELS; ch++ )
            k26->keep_on_at_end[ ch ] = true;

    return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_reset( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE == TEST )
        default_settings( );
    else if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_reset( );

    return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_reset_on_start( Var_T * v )
{
    k26->reset_on_start = true;

    if ( v )
    {
        k26->reset_on_start = get_boolean( v );
        too_many_arguments( v );
    }

    return vars_push( INT_VAR, k26->reset_on_start ? 1L: 0L );
}


/*--------------------------------------------------------------*
 * Returns or sets the sense mode, i.e. 2- or 4-wire measurements
 * (or calibration)
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_sense_mode( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( INT_VAR, ( long ) k26->sense[ ch ] );

    long int sense;
    if ( v->type == STR_VAR )
    {
        if (    ! strcasecmp( v->val.sptr, "LOCAL" )
             || ! strcasecmp( v->val.sptr, "2-WiRE" ) )
            sense = SENSE_LOCAL;
        else if (    ! strcasecmp( v->val.sptr, "REMOTE" )
                  || ! strcasecmp( v->val.sptr, "4-WiRE" ) )
            sense = SENSE_REMOTE;
        else if ( ! strcasecmp( v->val.sptr, "CALIBRATION" ) )
            sense = SENSE_CALA;
        else
        {
            print( FATAL, "Invalid sense mode argument: \"%s\".\n",
                   v->val.sptr );
            THROW( EXCEPTION );
        }
    }
    else
        sense = get_strict_long( v,"sense mode" );

    if ( sense != SENSE_LOCAL && sense != SENSE_REMOTE && sense != SENSE_CALA )
    {
        print( FATAL, "Invalid sense mode argument: %d.\n", sense );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_sense( ch, sense );
    else
        k26->sense[ ch ] = sense;

    return vars_push( INT_VAR, ( long ) k26->sense[ ch ] );
}


/*--------------------------------------------------------------*
 * Returns source output off-mode for the given channel or sets it
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_offmode( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( INT_VAR, ( long ) k26->source[ ch ].offmode );

    long int mode;
    if ( v->type == STR_VAR )
    {
        if ( ! strcasecmp( v->val.sptr, "NORMAL" ) )
            mode = OUTPUT_NORMAL;
        else if (    ! strcasecmp( v->val.sptr, "HIGH_Z" )
                  || ! strcasecmp( v->val.sptr, "HIGHZ" ) )
            mode = OUTPUT_HIGH_Z;
        else if ( ! strcasecmp( v->val.sptr, "ZERO" ) )
            mode = OUTPUT_ZERO;
        else
        {
            print( FATAL, "Invalid source off-mode argument: \"%s\".\n",
                   v->val.sptr );
            THROW( EXCEPTION );
        }
    }
    else
        mode = get_strict_long( v, "spurce mode" ) != 0;

    too_many_arguments( v );

    if ( mode < OUTPUT_NORMAL || mode > OUTPUT_ZERO )
    {
        print( FATAL, "Invalid source off-mode argument: %d.\n", mode );
        THROW( EXCEPTION );
    }

    if ( mode == k26->source[ ch ].offmode )
        return vars_push( INT_VAR, mode );

    if (  FSC2_MODE == EXPERIMENT )
        keithley2612b_set_source_offmode( ch, mode );
    else
        k26->source[ ch ].offmode = mode;

    return vars_push( INT_VAR, ( long ) k26->source[ ch ].offmode );
}


/*--------------------------------------------------------------*
 * Returns source output state for the given channel or switches
 * it on or off
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_output_state( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( INT_VAR, k26->source[ ch ].output ? 1L : 0L );

    bool on_off = get_boolean( v );
    too_many_arguments( v );

    if ( k26->source[ ch ].output == on_off )
        return vars_push( INT_VAR, on_off ? 1L : 0L );

    /* Before switching the channel on check that settings don't conflict */

    if ( ! keithley2612b_test_toggle_source_output( ch ) )
    {
        pp_buf bufs[ 3 ];

        if ( k26->source[ ch ].func == OUTPUT_DCAMPS )
            print( FATAL, "Can't switch on %sas a current source, "
                   "combination of current level (%s), range (%s) and "
                   "compliance voltage (%s) is not possible.\n",
                   ppc( ch, "" ), pp_a( k26->source[ ch ].leveli, bufs[ 0 ] ),
                   pp_a( k26->source[ ch ].rangei, bufs[ 1 ] ),
                   pp_v( k26->source[ ch ].levelv, bufs[ 2 ] ) );
        else
            print( FATAL, "Can't switch on %sas a voltage source, "
                   "combination of voltage level (%s), range (%s) and "
                   "compliance current (%s) is not possible.\n",
                   ppc( ch, "" ), pp_v( k26->source[ ch ].levelv, bufs[ 0 ] ),
                   pp_v( k26->source[ ch ].rangev, bufs[ 1 ] ),
                   pp_a( k26->source[ ch ].leveli, bufs[ 2 ] ) );

        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_source_output( ch, on_off );
    else
        k26->source[ ch ].output = on_off;

    return vars_push( INT_VAR, k26->source[ ch ].output ? 1L : 0L );
}


/*--------------------------------------------------------------*
 * Returns or sets the source mode (voltage or current source)
 * for the given channel
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_mode( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( INT_VAR,
                          k26->source[ ch ].func == OUTPUT_DCAMPS ? 0L : 1L );

    bool mode;
    if ( v->type == STR_VAR )
    {
        if (    ! strcasecmp( v->val.sptr, "CURRENT" )
             || ! strcasecmp( v->val.sptr, "AMPS" ) )
            mode = OUTPUT_DCAMPS;
        else if (    ! strcasecmp( v->val.sptr, "VOLTAGE" )
                  || ! strcasecmp( v->val.sptr, "VOLTS" ) )
            mode = OUTPUT_DCVOLTS;
        else
        {
            print( FATAL, "Invalid source mode argument: \"%s\"\n",
                   v->val.sptr );
            THROW( EXCEPTION );
        }
    }
    else
        mode = get_strict_long( v, "spurce mode" ) != 0;

    if ( keithley2612b.source[ ch ].func == mode )
        return vars_push( INT_VAR, mode == OUTPUT_DCAMPS ? 0L : 1L );

    too_many_arguments( v );

    if ( mode == k26->source[ ch ].func )
        return vars_push( INT_VAR, mode == OUTPUT_DCAMPS ? 0L : 1L );

    /* before switching the source function we need to check that all
       settings for the new mode are correct */

    if ( ! keithley2612b_test_toggle_source_func( ch ) )
    {
        pp_buf bufs[ 3 ];

        if ( mode == OUTPUT_DCAMPS )
            print( FATAL, "Can't switch %sto current source mode, combination "
                   "of current level (%s), range (%s) and compliance voltage "
                   "(%s) is not possible.\n", ppc( ch, "" ),
                   pp_a( k26->source[ ch ].leveli, bufs[ 0 ] ),
                   pp_a( k26->source[ ch ].rangei, bufs[ 1 ] ),
                   pp_v( k26->source[ ch ].levelv, bufs[ 2 ] ) );
        else
            print( FATAL, "Can't switch %sto current source mode, combination "
                   "of voltage level (%s), range (%s) and compliance current "
                   "(%s) is not possible.\n", ppc( ch, "" ),
                   pp_v( k26->source[ ch ].levelv, bufs[ 0 ] ),
                   pp_v( k26->source[ ch ].rangev, bufs[ 1 ] ),
                   pp_a( k26->source[ ch ].leveli, bufs[ 2 ] ) );

        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_source_func( ch, mode );
    else
        keithley2612b.source[ ch ].func = mode;

    return vars_push( INT_VAR,
                      k26->source[ ch ].func == OUTPUT_DCAMPS ? 0L : 1L );
}


/*--------------------------------------------------------------*
 * Returns source voltage or sets it for the given channel
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_voltage( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].levelv );

    double volts = get_double( v, NULL );
    too_many_arguments( v );

    if ( ! keithley2612b_check_source_levelv( ch, volts ) )
    {
        pp_buf bufs[ 2 ];
        print( FATAL, "Voltage of %s %sis out of currently possible range, "
               "may not exceed +/-%s.\n",
               pp_v( volts, bufs[ 0 ] ),
               ppc( ch, "for" ),
               pp_v( keithley2612b_max_source_levelv( ch ), bufs[ 1 ] ) );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_source_levelv( ch, volts );
    else
    {
        k26->source[ ch ].levelv = volts;
        if ( k26->source[ ch ].autorangev )
            k26->source[ ch ].rangev =
                                 keithley2612b_best_source_rangev( ch, volts );
    }

    return vars_push( FLOAT_VAR, k26->source[ ch ].levelv );
}


/*--------------------------------------------------------------*
 * Returns source current or sets it for the given channel
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_current( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].leveli );

    double amps = get_double( v, NULL );
    too_many_arguments( v );

    if ( ! keithley2612b_check_source_leveli( ch, amps ) )
    {
        pp_buf bufs[ 2 ];
        print( FATAL, "Source current of %s %sis out of currently possible "
               "range, may not exceed +/-%s.\n",
               pp_a( amps, bufs[ 0 ] ), ppc( ch, "for" ),
               pp_a( keithley2612b_max_source_leveli( ch ), bufs[ 1 ] ) );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_source_leveli( ch, amps );
    else
    {
        k26->source[ ch ].leveli = amps;
        if ( k26->source[ ch ].autorangei )
            k26->source[ ch ].rangei =
                                  keithley2612b_best_source_rangei( ch, amps );
    }

    return vars_push( FLOAT_VAR, k26->source[ ch ].leveli );
}


/*--------------------------------------------------------------*
 * Returns source voltage range for the given channel or sets it
 * (the value set is the lowest range that can be set under the
 * current circumstances with the requested value used as the
 * lower bound).
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_voltage_range( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].rangev );

    double range = fabs( get_double( v, NULL ) );
    too_many_arguments( v );

    /* Get the next range that can be set under the current circumstances,
       i.e. the range setting that's at last as large as the requested
       value or, when we're in voltage sourcing mode and output is on,
       the next range that covers the maximum of the requested value and
       the voltage level. */

    range = keithley2612b_best_source_rangev( ch, range );

    /* Check that requested range isn't out of bounds, otherwise correct it */

    if ( range < 0 )
        range = keithley2612b_max_source_rangev( ch );

    if ( FSC2_MODE == EXPERIMENT )
    {
        /* Don't try to set a lower range than the active voltage level
           if output is on and we're in voltage sourcing mode */

        if (    k26->source[ ch ].output
             && k26->source[ ch ].func == OUTPUT_DCVOLTS )
            range = d_max( range, k26->source[ ch ].levelv );

        keithley2612b_set_source_rangev( ch, range );
    }
    else
    {
        k26->source[ ch ].rangev = range;
        k26->source[ ch ].autorangev = false;
    }

    return vars_push( FLOAT_VAR, k26->source[ ch ].rangev );
}


/*--------------------------------------------------------------*
 * Returns source current range for the given channel or sets it
 * (the value set is the lowest range that can be set under the
 * current circumstances with the requested value used as the
 * lower bound).
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_current_range( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].rangei );

    double range = fabs( get_double( v, NULL ) );
    too_many_arguments( v );

    /* Get the next range that can be set under the current circumstances,
       i.e. the range setting that's at last as large as the requested
       value or, when we're in current sourcing mode and output is on,
       the next range that covers the maximum of the requested value and
       the current level. */

    range = keithley2612b_best_source_rangei( ch, range );

    /* Check that requested range isn't out of bounds, otherwise correct it */

    if ( range < 0 )
        range = keithley2612b_max_source_rangei( ch );

    if ( FSC2_MODE == EXPERIMENT )
    {
        /* Don't try to set a lower range than the active current level
           if output is on and we're in current sourcing mode */

        if (    k26->source[ ch ].output
             && k26->source[ ch ].func == OUTPUT_DCAMPS )
            range = d_max( range, k26->source[ ch ].leveli );

        keithley2612b_set_source_rangei( ch, range );
    }
    else
    {
        k26->source[ ch ].rangei = range;
        k26->source[ ch ].autorangei = false;
    }

    return vars_push( FLOAT_VAR, k26->source[ ch ].rangei );
}


/*--------------------------------------------------------------*
 * Returns or sets if source voltage autoranging is on or off.
 * Switching it on can change the source range setting.
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_voltage_autoranging( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( INT_VAR, k26->source[ ch ].autorangev ? 1L : 0L );

    bool on_off = get_boolean( v );
    too_many_arguments( v );

    if ( on_off == k26->source[ ch ].autorangev )
        return vars_push( INT_VAR, on_off ? 1L : 0L );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_source_autorangev( ch, on_off );
    else
    {
        /* Keep in mind that switching autoranging on may change the range
           setting */

        k26->source[ ch ].autorangev = on_off;
        if ( on_off )
            k26->source[ ch ].rangev =
                  keithley2612b_best_source_rangev( ch,
                                                    k26->source[ ch ].levelv );
    }

    return vars_push( INT_VAR, k26->source[ ch ].autorangev ? 1L : 0L );
}


/*--------------------------------------------------------------*
 * Returns or sets if source current autoranging is on or off
 * Switching it on can change the source range setting.
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_current_autoranging( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( INT_VAR, k26->source[ ch ].autorangei ? 1L : 0L );

    bool on_off = get_boolean( v );
    too_many_arguments( v );

    if ( on_off == k26->source[ ch ].autorangei )
        return vars_push( INT_VAR, on_off ? 1L : 0L );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_source_autorangei( ch, on_off );
    else
    {
        /* Keep in mind that switching autoranging on may change the range
           setting */

        k26->source[ ch ].autorangei = on_off;
        if ( on_off )
            k26->source[ ch ].rangei =
                  keithley2612b_best_source_rangei( ch,
                                                    k26->source[ ch ].leveli );
    }

    return vars_push( INT_VAR, k26->source[ ch ].autorangev ? 1L : 0L );
}


/*--------------------------------------------------------------*
 * Returns the source voltage autoranging low limit for the channel
 * or sets it (the requested value may get modified if necessary -
 * it it's too large it gets set to the maximum possible value,
 * otherwise it's set to next larger possible value). Note that
 * setting it may also change the source range setting when the
 * low limit is larger and autoranging is on.
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_voltage_autorange_low_limit( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].lowrangev );

    double lowrange = fabs( get_double( v, NULL ) );
    too_many_arguments( v );

    /* Find nearest possible range */

    lowrange = keithley2612b_best_source_rangev( ch, lowrange );

    /* A negative result indicates that the value was too large and we
       correct it down to the maximum */

    if ( lowrange < 0 )
        lowrange = keithley2612b_max_source_rangev( ch );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_source_lowrangev( ch, lowrange );
    else
    {
        k26->source[ ch ].lowrangev = lowrange;
        if ( k26->source[ ch ].autorangev )
            k26->source[ ch ].rangev =
                                   d_max( lowrange, k26->source[ ch ].rangev );
    }

    return vars_push( FLOAT_VAR, k26->source[ ch ].lowrangev );
}


/*--------------------------------------------------------------*
 * Returns the source current autoranging low limit for the channel
 * or sets it (the requested value may get modified if necessary -
 * it it's too large it gets set to the maximum possible value,
 * otherwise it's set to next larger possible value). Note that
 * setting it may also change the source range setting when the
 * low limit is larger and autoranging is on.
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_current_autorange_low_limit( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].lowrangei );

    double lowrange = fabs( get_double( v, NULL ) );
    too_many_arguments( v );

    /* Find nearest possible range */

    lowrange = keithley2612b_best_source_rangei( ch, lowrange );

    /* A negative result indicates that the value was too large and we
       correct it down to the maximum */

    if ( lowrange < 0 )
        lowrange = keithley2612b_max_source_rangei( ch );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_measure_rangei( ch, lowrange );
    else
    {
        k26->source[ ch ].lowrangei = lowrange;
        if ( k26->source[ ch ].autorangei )
            k26->source[ ch ].rangei =
                                  d_max( lowrange, k26->source[ ch ].rangei );
    }

    return vars_push( FLOAT_VAR, k26->source[ ch ].lowrangei );
}


/*--------------------------------------------------------------*
 * Returns or sets the voltage compliance limit
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_compliance_voltage( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].limitv );

    /* Accept negative values, just make 'em positive */

    double limit = fabs( get_double( v, NULL ) );
    too_many_arguments( v );
    
    if ( limit == k26->source[ ch ].limitv )
        return vars_push( FLOAT_VAR, limit );

    if ( ! keithley2612b_check_source_limitv( ch, limit ) )
    {
        pp_buf bufs[ 3 ];
        print( FATAL, "Requested compliance voltage %sof %s is out of range, "
               "must be between %s and %s under current circumstances.\n",
               ppc( ch, "for" ), pp_v( limit, bufs[ 0 ] ),
               pp_v( keithley2612b_min_source_limitv( ch ), bufs[ 1 ] ),
               pp_v( keithley2612b_max_source_limitv( ch ), bufs[ 2 ] ) );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_source_limitv( ch, limit );
    else
        k26->source[ ch ].limitv = limit;

    return vars_push( FLOAT_VAR, k26->source[ ch ].limitv );
}


/*--------------------------------------------------------------*
 * Returns or sets the current compliance limit
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_compliance_current( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].limiti );

    /* Accept negative values, jut make 'em positive */

    double limit = fabs( get_double( v, NULL ) );
    too_many_arguments( v );
    
    if ( limit == k26->source[ ch ].limiti )
        return vars_push( FLOAT_VAR, limit );

    if ( ! keithley2612b_check_source_limiti( ch, limit ) )
    {
        pp_buf bufs[ 3 ];
        print( FATAL, "Requested compliance current %sof %s is out of range, "
               "must be between %s and %s under current circumstances.\n",
               ppc( ch, "for" ), pp_a( limit, bufs[ 0 ] ),
               pp_a( keithley2612b_min_source_limiti( ch ), bufs[ 1 ] ),
               pp_a( keithley2612b_max_source_limiti( ch ), bufs[ 2 ] ) );

        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_source_limiti( ch, limit );
    else
        k26->source[ ch ].limiti = limit;

    return vars_push( FLOAT_VAR, k26->source[ ch ].limiti );
}


/*--------------------------------------------------------------*
 * Returns if the compliance limit is reached
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_test_compliance( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        return vars_push( INT_VAR,
                          keithley2612b_get_compliance( ch ) ? 1L : 0L );

    return vars_push( INT_VAR, TEST_COMPLIANCE );
}



/*--------------------------------------------------------------*
 * Returns or sets the source delay
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_delay( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].delay );

    double delay = get_double( v, NULL );

    if ( delay < 0 )
        delay = DELAY_AUTO;

    if ( delay == k26->source[ ch ].delay )
        vars_push( FLOAT_VAR, k26->source[ ch ].delay );

    /* There seems to be an (undocumented) limit on the maximum source delay
       that, when exceeded, results in the device becoming unresponsive */

    if ( delay > MAX_SOURCE_DELAY )
    {
        print( FATAL, "Source delay of %.3f s too long, must be less than "
               "%.3f s.\n", delay, MAX_SOURCE_DELAY );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_source_delay( ch, delay );
    else
        k26->source[ ch ].delay = delay;

    return vars_push( FLOAT_VAR, k26->source[ ch ].delay );
}


/*--------------------------------------------------------------*
 * Returns if source high capacity state for the given channel
 * is on or off or switches it on or off
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_high_capacity( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( v )
    {
        bool highc = get_boolean( v );

        if ( FSC2_MODE == EXPERIMENT )
            keithley2612b_set_source_highc( ch, highc );
        else
        {
            k26->source[ ch ].highc = highc;
            correct_for_highc( ch );
        }
    }

    return vars_push( INT_VAR, k26->source[ ch ].highc ? 1L : 0L );
}


/*--------------------------------------------------------------*
 * Returns or sets the source sink mode of the channel
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_sink_mode( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( INT_VAR, k26->source[ ch ].sink ? 1L : 0L );

    bool sink = get_boolean( v );
    too_many_arguments( v );

    if ( sink == k26->source[ ch ].sink )
        return vars_push( INT_VAR, sink ? 1L : 0L );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_source_sink( ch, sink );
    else
        k26->source[ ch ].sink = sink;

    return vars_push( INT_VAR, k26->source[ ch ].sink ? 1L : 0L );
}


/*--------------------------------------------------------------*
 * Returns or sets the source settling mode of the channel
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_settling_mode( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( INT_VAR, ( long ) k26->source[ ch ].settling );

    long int settle;
    if ( v->type == STR_VAR )
    {
        if ( ! strcasecmp( v->val.sptr, "SMOOTH" ) )
             settle = SETTLE_SMOOTH;
        else if ( ! strcasecmp( v->val.sptr, "FAST_RANGE" ) )
            settle = SETTLE_FAST_RANGE;
        else if ( ! strcasecmp( v->val.sptr, "FAST_POLARITY" ) )
            settle = SETTLE_FAST_POLARITY;
        else if ( ! strcasecmp( v->val.sptr, "DIRECT_IRANGE" ) )
            settle = SETTLE_DIRECT_IRANGE;
        else if ( ! strcasecmp( v->val.sptr, "SMOOTH_100NA" ) )
            settle = SETTLE_SMOOTH_100NA;
        else if ( ! strcasecmp( v->val.sptr, "FAST_ALL" ) )
            settle = SETTLE_FAST_ALL;
        else
        {
            print( FATAL, "Invalid settling mode argument: \"%s\".\n",
                   v->val.sptr );
            THROW( EXCEPTION );
        }
    }
    else
        settle = get_strict_long( v, "source_settling mode" );

    too_many_arguments( v );

    if (    settle != SETTLE_SMOOTH
         && settle != SETTLE_FAST_RANGE
         && settle != SETTLE_FAST_POLARITY
         && settle != SETTLE_DIRECT_IRANGE
         && settle != SETTLE_SMOOTH_100NA
         && settle != SETTLE_FAST_ALL )
    {
        print( FATAL, "Invalid settling mode argument: %d.\n", settle );
        THROW( EXCEPTION );
    }

    if ( settle == k26->source[ ch ].settling )
        return vars_push( INT_VAR, settle );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_source_settling( ch,settle );
    else
        k26->source[ ch ].settling = settle;

    return vars_push( INT_VAR, ( long ) k26->source[ ch ].settling );
}


/*--------------------------------------------------------------*
 * Returns or sets the maximum source current when in normal off state
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_max_off_current( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].offlimiti );

    double limit = fabs( get_double( v, NULL ) );
    too_many_arguments( v );

    if ( limit == k26->source[ ch ].offlimiti )
         return vars_push( FLOAT_VAR, limit );

    if ( ! keithley2612b_check_source_offlimiti( ch, limit ) )
    {
        pp_buf bufs[ 3 ];
        print( FATAL, "Requested maximum current in normal off state %sof %s "
               "out of range, must be between %s and %s.\n",
               ppc( ch, "for" ), pp_a( limit, bufs[ 0 ] ),
               pp_a( keithley2612b_min_source_offlimiti( ch ), bufs[ 1 ] ),
               pp_a( keithley2612b_max_source_offlimiti( ch ), bufs[ 2 ] ) );

        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_source_offlimiti( ch, limit );
    else
        k26->source[ ch ].offlimiti = limit;

    return vars_push( FLOAT_VAR, k26->source[ ch ].offlimiti );
}                


/*--------------------------------------------------------------*
 * Returns a voltage reading
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_voltage( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    too_many_arguments( v );

    double volts;

    if ( FSC2_MODE == EXPERIMENT )
    {
        volts = keithley2612b_measure( ch, VOLTAGE );

        if ( fabs( volts ) >= 9.9e37 )
            print( WARN, "Voltage to be measure out of range.\n" );
    }
    else
    {
        if ( k26->measure[ ch ].autorangev )
            k26->measure[ ch ].rangev =
                          keithley2612b_best_measure_rangev( ch, TEST_VOLTAGE );

        volts =   TEST_VOLTAGE
                - ( k26->measure[ ch ].relv.enabled ?
                    k26->measure[ ch ].relv.level : 0 );
    }

    return vars_push( FLOAT_VAR, volts );
}


/*--------------------------------------------------------------*
 * Returns a current reading
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_current( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    too_many_arguments( v );

    double amps;
    
    if ( FSC2_MODE == EXPERIMENT )
    {
        amps = keithley2612b_measure( ch, CURRENT );

        if ( fabs( amps ) >= 9.9e37 )
            print( WARN, "Current to be measure out of range.\n" );
    }
    else
    {
        if ( k26->measure[ ch ].autorangei )
            k26->measure[ ch ].rangei =
                         keithley2612b_best_measure_rangei( ch, TEST_CURRENT );

        amps =   TEST_CURRENT
               - ( k26->measure[ ch ].reli.enabled ?
                   k26->measure[ ch ].reli.level : 0 );
    }

    return vars_push( FLOAT_VAR, amps );
}


/*--------------------------------------------------------------*
 * Returns a power reading
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_power( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( k26->measure[ ch ].autorangev )
            k26->measure[ ch ].rangev =
                          keithley2612b_best_measure_rangev( ch,TEST_VOLTAGE );

        if ( k26->measure[ ch ].autorangei )
            k26->measure[ ch ].rangei =
                          keithley2612b_best_measure_rangei( ch,TEST_CURRENT );

        return vars_push( FLOAT_VAR, TEST_VOLTAGE * TEST_CURRENT );
    }

    double p = keithley2612b_measure( ch, POWER );

    if ( fabs( p ) >= 9.9e37 )
        print( WARN, "Out of range condition while measuring power.\n" );

    return vars_push( FLOAT_VAR, p );
}


/*--------------------------------------------------------------*
 * Returns a resistance reading
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_resistance( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( k26->measure[ ch ].autorangev )
            k26->measure[ ch ].rangev =
                          keithley2612b_best_measure_rangev( ch,TEST_VOLTAGE );

        if ( k26->measure[ ch ].autorangei )
            k26->measure[ ch ].rangei =
                          keithley2612b_best_measure_rangei( ch,TEST_CURRENT );

        return vars_push( FLOAT_VAR, TEST_VOLTAGE / TEST_CURRENT );
    }

    double r = keithley2612b_measure( ch, RESISTANCE );

    if ( fabs( r ) > 9.9e37 )
        print( WARN, "Out of range condition while measuring resistance.\n" );

    return vars_push( FLOAT_VAR, r );
}


/*--------------------------------------------------------------*
 * Returns an array with a voltage and a current reading taken in
 * parallel
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_voltage_and_current( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( k26->measure[ ch ].autorangev )
            k26->measure[ ch ].rangev =
                          keithley2612b_best_measure_rangev( ch,TEST_VOLTAGE );
        if ( k26->measure[ ch ].autorangei )
            k26->measure[ ch ].rangei =
                          keithley2612b_best_measure_rangei( ch,TEST_CURRENT );

        double vi[ ] = { TEST_VOLTAGE, TEST_CURRENT };
        return vars_push( FLOAT_ARR, vi, 2L );
    }

    const double * r = keithley2612b_measure_iv( ch );
    double vi[ ] = { r[ 1 ], r[ 0 ] };

    if ( fabs( vi[ 0 ] ) >= 9.9e37 || fabs( vi[ 1 ] ) >= 9.9e37 )
        print( WARN, "Measured voltage or current out of range.\n" );

    return vars_push( FLOAT_ARR, vi, 2L );
}


/*--------------------------------------------------------------*
 * Returns measure voltage range for the given channel or sets it
 * (the value set is the lowest range that can be set under the
 * current circumstances with the requested value used as the
 * lower bound). Note that in voltage sourcing mode the source
 * voltage range "overrules" the settings for the measure range.
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_voltage_range( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
    {
        if (    FSC2_MODE == EXPERIMENT
             && k26->measure[ ch ].autorangev )
            return vars_push( FLOAT_VAR,
                              keithley2612b_get_measure_rangev( ch ) );

        if ( k26->source[ ch ].func == OUTPUT_DCVOLTS )
            return vars_push( FLOAT_VAR, k26->source[ ch ].rangev );

        return vars_push( FLOAT_VAR, k26->measure[ ch ].rangev );
    }

    double range = fabs( get_double( v, NULL ) );
    too_many_arguments( v );

    /* Get the range setting that's at last as large as the requested value. */

    range = keithley2612b_best_measure_rangev( ch, range );

    /* Check that requested range isn't out of bounds, otherwise correct it */

    if ( range < 0 )
        range = keithley2612b_max_measure_rangev( ch );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_measure_rangev( ch, range );
    else
    {
        k26->measure[ ch ].rangev = range;
        k26->measure[ ch ].autorangev = false;
    }

    /* If the device is in voltage sourcing mode it keeps the measure
       range identical to the source range! */

    if ( k26->source[ ch ].func == OUTPUT_DCVOLTS )
        return vars_push( FLOAT_VAR, k26->source[ ch ].rangev );

    return vars_push( FLOAT_VAR, k26->measure[ ch ].rangev );
}


/*--------------------------------------------------------------*
 * Returns measure current range for the given channel or sets it
 * (the value set is the lowest range that can be set under the
 * current circumstances with the requested value used as the
 * lower bound). Note that in current sourcing mode the source
 * current range "overrules" the settings for the measure range.
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_current_range( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
    {
        if (    FSC2_MODE == EXPERIMENT
             && k26->measure[ ch ].autorangei )
            return vars_push( FLOAT_VAR,
                              keithley2612b_get_measure_rangei( ch ) );

        if ( k26->source[ ch ].func == OUTPUT_DCAMPS )
            return vars_push( FLOAT_VAR, k26->source[ ch ].rangei );

        return vars_push( FLOAT_VAR,  k26->measure[ ch ].rangei );
    }

    double range = fabs( get_double( v, NULL ) );
    too_many_arguments( v );

    /* Get the range setting that's at last as large as the requested value. */

    range = keithley2612b_best_measure_rangei( ch, range );

    /* Check that requested range isn't out of bounds, otherwise correct it */

    if ( range < 0 )
        range = keithley2612b_max_measure_rangei( ch );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_measure_rangei( ch, range );
    else
    {
        k26->measure[ ch ].rangei = range;
        k26->measure[ ch ].autorangei = false;
    }

    /* If the device is in current sourcing mode it keeps the measure
       range identical to the source range! */

    if ( k26->source[ ch ].func == OUTPUT_DCAMPS )
        return vars_push( FLOAT_VAR, k26->source[ ch ].rangei );

    return vars_push( FLOAT_VAR, k26->measure[ ch ].rangei );
}


/*--------------------------------------------------------------*
 * Returns if voltage measure autoranging is on or off or sets it
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_voltage_autoranging( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( INT_VAR, k26->measure[ ch ].autorangev ? 1L : 0L );

    bool on_off = get_boolean( v );
    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_measure_autorangev( ch, on_off );
    else
        k26->measure[ ch ].autorangev = on_off;

    return vars_push( INT_VAR, k26->measure[ ch ].autorangev ? 1L : 0L );
}


/*--------------------------------------------------------------*
 * Returns if current measure autoranging is on or off or sets it
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_current_autoranging( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( INT_VAR, k26->measure[ ch ].autorangei ? 1L : 0L );

    bool on_off = get_boolean( v );
    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_measure_autorangei( ch, on_off );
    else
        k26->measure[ ch ].autorangei = on_off;

    return vars_push( INT_VAR, k26->measure[ ch ].autorangei ? 1L : 0L );
}


/*--------------------------------------------------------------*
 * Returns the measurement voltage autoranging low limit for the
 * channel or sets it (the requested value may get modified if
 * necessary - if it's too large it gets set to the maximum
 * possible value, otherwise it's set to next larger possible
 * value). Note that setting it may also change the measurement
 * voltage range setting when the low limit is larger and
 * autoranging is on.
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_voltage_autorange_low_limit( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->measure[ ch ].lowrangev );

    double lowrange = fabs( get_double( v, NULL ) );
    too_many_arguments( v );

    /* Find nearest possible range */

    lowrange = keithley2612b_best_measure_rangev( ch, lowrange );

    /* A negative result indicates that the value was too large and we
       correct it down to the maximum */

    if ( lowrange < 0 )
        lowrange = keithley2612b_max_measure_rangev( ch );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_measure_lowrangev( ch, lowrange );
    else
    {
        k26->measure[ ch ].lowrangev = lowrange;
        if ( k26->measure[ ch ].autorangev )
            k26->measure[ ch ].rangev =
                                  d_max( lowrange, k26->measure[ ch ].rangev );
    }

    return vars_push( FLOAT_VAR, k26->measure[ ch ].lowrangev );
}


/*--------------------------------------------------------------*
 * Returns the measurement current autoranging low limit for the
 * channel or sets it (the requested value may get modified if
 * necessary - if it's too large it gets set to the maximum
 * possible value, otherwise it's set to next larger possible
 * value). Note that setting it may also change the measurment
 * current range setting when the low limit is larger and
 * autoranging is on.
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_current_autorange_low_limit( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->measure[ ch ].lowrangei );

    double lowrange = fabs( get_double( v, NULL ) );
    too_many_arguments( v );

    /* Find nearest possible range */

    lowrange = keithley2612b_best_measure_rangei( ch, lowrange );

    /* A negative result indicates that the value was too large and we
       correct it down to the maximum */

    if ( lowrange < 0 )
        lowrange = keithley2612b_max_measure_rangei( ch );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_measure_lowrangei( ch, lowrange );
    else
    {
        k26->measure[ ch ].lowrangei = lowrange;
        if ( k26->measure[ ch ].autorangei )
            k26->measure[ ch ].rangei =
                                  d_max( lowrange, k26->measure[ ch ].rangei );
    }

    return vars_push( FLOAT_VAR, k26->measure[ ch ].lowrangei );
}


/*--------------------------------------------------------------*
 * Returns or sets the integration time for measurements with
 * the given channel (possible range depends on line frequence,
 * must be between 0.001 and 25 times the line period)
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_time( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->measure[ ch ].time );

    double t = get_double( v, NULL );
    too_many_arguments( v );

    if ( ! keithley2612b_check_measure_time( t ) )
    {
        pp_buf bufs[ 3 ];
        print( FATAL, "Requested measure time of %s %snot possible, must "
               "be between %s and %s.\n", pp_s( t, bufs[ 0 ] ),
               ppc( ch, "for" ),
               pp_s( keithley2612b_min_measure_time( ), bufs[ 1 ] ),
               pp_s( keithley2612b_max_measure_time( ), bufs[ 2 ] ) );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_measure_time( ch, t );
    else
        k26->measure[ ch ].time = t;

    return vars_push( FLOAT_VAR, k26->measure[ ch ].time );
} 


/*--------------------------------------------------------------*
 * Returns or sets an offset for the measured voltages
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_voltage_offset( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->measure[ ch ].relv.enabled ?
                                     k26->measure[ ch ].relv.level : 0.0 );

    double offset = get_double( v, NULL );
    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
    {
        if ( offset == 0.0 )
            keithley2612b_set_measure_rel_levelv_enabled( ch, false );
        else
        {
            keithley2612b_set_measure_rel_levelv( ch, offset );
            if ( ! k26->measure[ ch ].relv.enabled )
                keithley2612b_set_measure_rel_levelv_enabled( ch, true );
        }
    }
    else
    {
        if ( offset == 0.0 )
            k26->measure[ ch ].relv.enabled = false;
        else
        {
            k26->measure[ ch ].relv.level = offset;
            k26->measure[ ch ].relv.enabled = true;
        }
    }

    return vars_push( FLOAT_VAR, k26->measure[ ch ].relv.enabled ?
                                 k26->measure[ ch ].relv.level : 0.0 );
}
    

/*--------------------------------------------------------------*
 * Returns or sets an offset for the measured currents
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_current_offset( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->measure[ ch ].reli.enabled ?
                                     k26->measure[ ch ].reli.level : 0.0 );

    double offset = get_double( v, NULL );
    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
    {
        if ( offset == 0.0 )
            keithley2612b_set_measure_rel_leveli_enabled( ch, false );
        else
        {
            keithley2612b_set_measure_rel_leveli( ch, offset );
            if ( ! k26->measure[ ch ].reli.enabled )
                keithley2612b_set_measure_rel_leveli_enabled( ch, true );
        }
    }
    else
    {
        if ( offset == 0.0 )
            k26->measure[ ch ].reli.enabled = false;
        else
        {
            k26->measure[ ch ].reli.level = offset;
            k26->measure[ ch ].reli.enabled = true;
        }
    }

    return vars_push( FLOAT_VAR, k26->measure[ ch ].reli.enabled ?
                                 k26->measure[ ch ].reli.level : 0.0 );
}
    

/*--------------------------------------------------------------*
 * Returns or sets the measurement delay
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_delay( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->measure[ ch ].delay );

    double delay = get_double( v, NULL );

    if ( delay < 0 )
        delay = DELAY_AUTO;

    if ( delay == k26->measure[ ch ].delay )
        vars_push( FLOAT_VAR, k26->measure[ ch ].delay );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_measure_delay( ch, delay );
    else
        k26->measure[ ch ].delay = delay;

    return vars_push( FLOAT_VAR, k26->measure[ ch ].delay );
}


/*--------------------------------------------------------------*
 * Returns or sets the filter type for the measured voltages
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_filter_type( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( INT_VAR, ( long ) k26->measure[ ch ].filter.type );

    long int type;
    if ( v->type == STR_VAR )
    {
        if ( ! strcasecmp( v->val.sptr, "MOVING_AVG" ) )
            type = FILTER_MOVING_AVG;
        else if ( ! strcasecmp( v->val.sptr, "REPEAT_AVG" ) )
            type = FILTER_REPEAT_AVG;
        else if ( ! strcasecmp( v->val.sptr, "MEDIAN" ) )
            type = FILTER_MEDIAN;
        else
        {
            print( FATAL, "Unknown filter type: \"%s\".\n", v->val.sptr );
            THROW( EXCEPTION );
        }
    }
    else
    {
        type = get_strict_long( v, "filter type" );
        if (    type != FILTER_MOVING_AVG
             && type != FILTER_REPEAT_AVG
             && type != FILTER_MEDIAN )
        {
            print( FATAL, "Unknown filter type: %d.\n", type );
            THROW( EXCEPTION );
        }
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_measure_filter_type( ch, type );
    else
        k26->measure[ ch ].filter.type = type;

    return vars_push( INT_VAR, ( long ) k26->measure[ ch ].filter.type );
}


/*--------------------------------------------------------------*
 * Returns or sets the filter count for the measured voltages
 * (zero disables filter) 
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_measure_filter_count( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( INT_VAR, k26->measure[ ch ].filter.enabled ?
                          ( long ) k26->measure[ ch ].filter.count : 0L );

    long int count = get_strict_long( v, "filter count" );
    too_many_arguments( v );

    if ( count < 0 )
    {
        print( FATAL, "Invalid negative filter count.\n" );
        THROW( EXCEPTION );
    }

    if ( count > MAX_FILTER_COUNT )
    {
        print( FATAL, "Filter count of %ld too large, maximum is %d.\n",
               count, MAX_FILTER_COUNT );
    }

    if ( FSC2_MODE == EXPERIMENT )
    {
        if ( count == 0 )
            keithley2612b_set_measure_filter_enabled( ch, false );
        else
        {
            keithley2612b_set_measure_filter_count( ch, count );
            keithley2612b_set_measure_filter_enabled( ch, true );
        }
    }
    else
    {
        if ( count == 0 )
            k26->measure[ ch ].filter.enabled = false;
        else
        {
            k26->measure[ ch ].filter.count = count;
            k26->measure[ ch ].filter.enabled = true;
        }
    }

    return vars_push( INT_VAR, k26->measure[ ch ].filter.enabled ?
                               ( long ) k26->measure[ ch ].filter.count : 0L );
}


/*--------------------------------------------------------------*
 * Does a voltage sweep while measuring voltages
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_sweep_voltage_measure_voltage( Var_T * v )
{
    return do_sweep( v, VOLTAGE, VOLTAGE );
}


/*--------------------------------------------------------------*
 * Does a voltage sweep while measuring currents
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_sweep_voltage_measure_current( Var_T * v )
{
    return do_sweep( v, VOLTAGE, CURRENT );
}


/*--------------------------------------------------------------*
 * Does a voltage sweep while measuring powers
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_sweep_voltage_measure_power( Var_T * v )
{
    return do_sweep( v, VOLTAGE, POWER );
}


/*--------------------------------------------------------------*
 * Does a voltage sweep while measuring resistances
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_sweep_voltage_measure_resistance( Var_T * v )
{
    return do_sweep( v, VOLTAGE, RESISTANCE );
}


/*--------------------------------------------------------------*
 * Does a voltage sweep while simultaneously measuring voltages
 * and currents
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_sweep_voltage_measure_voltage_and_current( Var_T * v )
{
    return do_sweep( v, VOLTAGE, VOLTAGE_AND_CURRENT );
}


/*--------------------------------------------------------------*
 * Does a current sweep while measuring voltages
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_sweep_current_measure_voltage( Var_T * v )
{
    return do_sweep( v, CURRENT, VOLTAGE );
}


/*--------------------------------------------------------------*
 * Does a current sweep while measuring currents
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_sweep_current_measure_current( Var_T * v )
{
    return do_sweep( v, CURRENT, CURRENT );
}


/*--------------------------------------------------------------*
 * Does a current sweep while measuring powers
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_sweep_current_measure_power( Var_T * v )
{
    return do_sweep( v, CURRENT, POWER );
}


/*--------------------------------------------------------------*
 * Does a current sweep while measuring resistances
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_sweep_current_measure_resistance( Var_T * v )
{
    return do_sweep( v, CURRENT, RESISTANCE );
}


/*--------------------------------------------------------------*
 * Does a current sweep while simultaneously measuring voltages
 * and currents
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_sweep_current_measure_voltage_and_current( Var_T * v )
{
    return do_sweep( v, CURRENT, VOLTAGE_AND_CURRENT );
}


/*--------------------------------------------------------------*
 * Does a voltage list sweep while measuring voltages
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_list_sweep_voltage_measure_voltage( Var_T * v )
{
    return do_list_sweep( v, VOLTAGE, VOLTAGE );
}


/*--------------------------------------------------------------*
 * Does a voltage list sweep while measuring currents
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_list_sweep_voltage_measure_current( Var_T * v )
{
    return do_list_sweep( v, VOLTAGE, CURRENT );
}


/*--------------------------------------------------------------*
 * Does a voltage list sweep while measuring powers
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_list_sweep_voltage_measure_power( Var_T * v )
{
    return do_list_sweep( v, VOLTAGE, POWER );
}


/*--------------------------------------------------------------*
 * Does a voltage list_sweep while measuring resistances
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_list_sweep_voltage_measure_resistance( Var_T * v )
{
    return do_list_sweep( v, VOLTAGE, RESISTANCE );
}


/*--------------------------------------------------------------*
 * Does a voltage list sweep while simultaneously measuring voltages
 * and currents
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_list_sweep_voltage_measure_voltage_and_current( Var_T * v )
{
    return do_list_sweep( v, VOLTAGE, VOLTAGE_AND_CURRENT );
}


/*--------------------------------------------------------------*
 * Does a current list sweep while measuring voltages
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_list_sweep_current_measure_voltage( Var_T * v )
{
    return do_list_sweep( v, CURRENT, VOLTAGE );
}


/*--------------------------------------------------------------*
 * Does a current list sweep while measuring currents
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_list_sweep_current_measure_current( Var_T * v )
{
    return do_list_sweep( v, CURRENT, CURRENT );
}


/*--------------------------------------------------------------*
 * Does a current list sweep while measuring powers
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_list_sweep_current_measure_power( Var_T * v )
{
    return do_list_sweep( v, CURRENT, POWER );
}


/*--------------------------------------------------------------*
 * Does a current list sweep while measuring resistances
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_list_sweep_current_measure_resistance( Var_T * v )
{
    return do_list_sweep( v, CURRENT, RESISTANCE );
}


/*--------------------------------------------------------------*
 * Does a current list sweep while simultaneously measuring voltages
 * and currents
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_list_sweep_current_measure_voltage_and_current( Var_T * v )
{
    return do_list_sweep( v, CURRENT, VOLTAGE_AND_CURRENT );
}


/*--------------------------------------------------------------*
 * Does a contact check
 * (Not available on 2604B, 2614B, and 2634B.)
 *--------------------------------------------------------------*/

#if ! defined _2604B && ! defined _2614B || ! defined _2634B
Var_T *
sourcemeter_contact_check( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    too_many_arguments( v );

    if (    ! k26->source[ ch ].output
         && k26->source[ ch ].offmode == OUTPUT_HIGH_Z )
    {
        print( FATAL, "Contact check not possible while output is off in "
               "HIGH-Z ,ode.\n" );
        THROW( EXCEPTION );
    }

    if (    ! k26->source[ ch ].output
         && k26->source[ ch ].offlimiti < MIN_CONTACT_CURRENT_LIMIT )
    {
        pp_buf buf;
        print( FATAL, "Contact check not possible while current is limited "
               "to less than %s.\n", pp_a( MIN_CONTACT_CURRENT_LIMIT, buf ) );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        return vars_push( INT_VAR,
                          keithley2612b_contact_check( ch ) ? 1L : 0L );

    return vars_push( INT_VAR, 1L );
}
#endif


/*--------------------------------------------------------------*
 * Does a contact resistance measurement, returns lower resistance
 * value in first element of array.
 * (Not available on 2604B, 2614B, and 2634B.)
 *--------------------------------------------------------------*/

#if ! defined _2604B && ! defined _2614B || ! defined _2634B
Var_T *
sourcemeter_contact_resistance( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    too_many_arguments( v );

    if (    ! k26->source[ ch ].output
         && k26->source[ ch ].offmode == OUTPUT_HIGH_Z )
    {
        print( FATAL, "Contact resistance measurement not possible while "
               "output is off in HIGH-Z ,ode.\n" );
        THROW( EXCEPTION );
    }

    if (    k26->source[ ch ].output
         && k26->source[ ch ].leveli < MIN_CONTACT_CURRENT_LIMIT )
    {
        pp_buf buf;
        print( FATAL, "Contact resistance measurement not possible while "
               "output is on but current is limited to less than %s.\n",
               pp_a( MIN_CONTACT_CURRENT_LIMIT, buf ) );
        THROW( EXCEPTION );
    }

    if (    ! k26->source[ ch ].output
         && k26->source[ ch ].offlimiti < MIN_CONTACT_CURRENT_LIMIT )
    {
        pp_buf buf;
        print( FATAL, "Contact resistance measurement not possible while "
               "output is off and off-current is limited to less than %s.\n",
               pp_a( MIN_CONTACT_CURRENT_LIMIT, buf ) );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        double r12[ ] = { TEST_CONTACT_R_LOW, TEST_CONTACT_R_HIGH };
        return vars_push( FLOAT_ARR, r12, 2L );
    }

    const double * r = keithley2612b_contact_resistance( ch );
    double r12[ ] = { r[ 1 ], r[ 0 ] };

    return vars_push( FLOAT_ARR, r12, 2L );
}
#endif
       

/*--------------------------------------------------------------*
 * Returns or sets the resistance threshold used in contact measurements.
 * (Not available on 2604B, 2614B, and 2634B.)
 *--------------------------------------------------------------*/

#if ! defined _2604B && ! defined _2614B || ! defined _2634B
Var_T *
sourcemeter_contact_threshold( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->contact[ ch ].threshold );

    double threshold = get_double( v, NULL );
    too_many_arguments( v );

    if ( threshold < 0 )
    {
        print( FATAL, "Invalid negative contact resistance threshold.\n" );
        THROW( EXCEPTION );
    }

    if ( threshold > 1.0e37 )
        threshold = 1.0e37;

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_contact_threshold( ch, threshold );
    else
        k26->contact[ ch ].threshold = threshold;

    return vars_push( FLOAT_VAR, k26->contact[ ch ].threshold );
}
#endif


/*--------------------------------------------------------------*
 * Returns or sets the speed used in contact measurements.
 * (Not available on 2604B, 2614B, and 2634B.)
 *--------------------------------------------------------------*/

#if ! defined _2604B && ! defined _2614B || ! defined _2634B
Var_T *
sourcemeter_contact_speed( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( ! v )
        return vars_push( INT_VAR, k26->contact[ ch ].speed );

    long int speed;
    if ( v->type == STR_VAR )
    {
        if ( ! strcasecmp( v->val.sptr, "FAST" ) )
            speed = CONTACT_FAST;
        else if ( ! strcasecmp( v->val.sptr, "MEDIUM" ) )
            speed = CONTACT_MEDIUM;
        else if ( ! strcasecmp( v->val.sptr, "SLOW" ) )
            speed = CONTACT_SLOW;
        else
        {
            print( FATAL, "Invalid contact measurement speed argument: "
                   "\"%s\".\n", v->val.sptr );
            THROW( EXCEPTION );
        }
    }
    else
    {
        speed = get_long( v, "contact measurement speed.\n" );

        if ( speed < CONTACT_FAST || speed > CONTACT_SLOW )
        {
            print( FATAL, "Invalid contact measurement speed argument: %d\n",
                   speed );
            THROW( EXCEPTION );
        }
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2612b_set_contact_speed( ch, speed );
    else
        k26->contact[ ch ].speed = speed;

    return vars_push( INT_VAR, k26->contact[ ch ].speed );
}
#endif


/*--------------------------------------------------------------*
 * Helper function for doing a linear sweep
 *--------------------------------------------------------------*/

static
Var_T *
do_sweep( Var_T * v,
          int     sweep_what,
          int     measure_what )
{
    unsigned int ch;
    double start,
           end;
    int num_points;

    get_lin_sweep_params( v, sweep_what, measure_what, &ch, &start, &end,
                          &num_points );
    int num_data_points =   ( measure_what == VOLTAGE_AND_CURRENT ? 2 : 1 )
                          * num_points;

    double *data;

    if ( FSC2_MODE == EXPERIMENT )
    {
        data = keithley2612b_sweep_and_measure( ch, sweep_what, measure_what,
                                                start, end, num_points );
        check_sweep_data( data, num_data_points );
    }
    else
    {
        data = T_malloc( num_data_points * sizeof *data );

        if ( measure_what != VOLTAGE_AND_CURRENT )
        {
            double r;

            if ( measure_what == VOLTAGE )
                r =   TEST_VOLTAGE
                    - ( k26->measure[ ch ].relv.enabled ?
                        k26->measure[ ch ].relv.level : 0 );
            else if ( measure_what == CURRENT )
                r =   TEST_CURRENT
                    - ( k26->measure[ ch ].reli.enabled ?
                        k26->measure[ ch ].reli.level : 0 );
            else if ( measure_what == POWER )
                r = TEST_VOLTAGE * TEST_CURRENT;
            else
                r = TEST_VOLTAGE / TEST_CURRENT;

            for ( int i = 0; i < num_points; i++ )
                data[ i ] = r;
        }
        else
        {
            double volts =   TEST_VOLTAGE
                           - ( k26->measure[ ch ].relv.enabled ?
                               k26->measure[ ch ].relv.level : 0 );
            double amps  =   TEST_CURRENT
                           - ( k26->measure[ ch ].reli.enabled ?
                               k26->measure[ ch ].reli.level : 0 );

            for ( int i = 0; i < num_points; i++ )
            {
                data[ 2 * i     ] = volts;
                data[ 2 * i + 1 ] = amps;
            }
        }
    }

    v = vars_push( FLOAT_ARR, data, ( long ) num_data_points );
    T_free( data );
    return v;
}


/*--------------------------------------------------------------*
 * Helper function for doing a list sweep
 *--------------------------------------------------------------*/

static
Var_T *
do_list_sweep( Var_T * v,
               int     sweep_what,
               int     measure_what )
{
    unsigned int ch;

    v = get_list_sweep_params( v, sweep_what, measure_what, &ch );
    int num_data_points =   ( measure_what == VOLTAGE_AND_CURRENT ? 2 : 1 )
                          * v->len;

    double * data;

    if ( FSC2_MODE == EXPERIMENT )
    {
        data = keithley2612b_list_sweep_and_measure( ch, sweep_what,
                                                     measure_what, v );
        check_sweep_data( data, num_data_points );
    }
    else
    {
        data = T_malloc( num_data_points * sizeof *data );

        if ( measure_what != VOLTAGE_AND_CURRENT )
        {
            double r;
            if ( measure_what == VOLTAGE )
                r =   TEST_VOLTAGE
                    - ( k26->measure[ ch ].relv.enabled ?
                        k26->measure[ ch ].relv.level : 0 );
            else if ( measure_what == CURRENT )
                r =   TEST_CURRENT
                    - ( k26->measure[ ch ].reli.enabled ?
                        k26->measure[ ch ].reli.level : 0 );
            else if ( measure_what == POWER )
                r = TEST_VOLTAGE * TEST_CURRENT;
            else
                r = TEST_VOLTAGE / TEST_CURRENT;

            for ( int i = 0; i < num_data_points; i++ )
                data[ i ] = r;
        }
        else
        {
            double volts =   TEST_VOLTAGE
                           - ( k26->measure[ ch ].relv.enabled ?
                               k26->measure[ ch ].relv.level : 0 );
            double amps  =   TEST_CURRENT
                           - ( k26->measure[ ch ].reli.enabled ?
                               k26->measure[ ch ].reli.level : 0 );

            for ( int i = 0; i < num_data_points / 2; i++ )
            {
                data[ 2 * i     ] = volts;
                data[ 2 * i + 1 ] = amps;
            }
        }
    }

    v = vars_push( FLOAT_ARR, data, ( long ) num_data_points );
    T_free( data );
    return v;
}


/*--------------------------------------------------------------*
 * Helper function for extracting the arguments to the various
 * linear sweep functions
 *--------------------------------------------------------------*/

static
void
get_lin_sweep_params( Var_T        * v,
                      int            sweep_what,
                      int            measure_what,
                      unsigned int * ch,
                      double       * start,
                      double       * end,
                      int          * num_points )
{
    const char *what = sweep_what == VOLTAGE ? "voltage" : "current";

    *ch = get_channel( &v );

    if ( ! v )
    {
        print( FATAL, "Missing sweep start %s argument.\n", what );
        THROW( EXCEPTION );
    }

    *start = get_double( v, NULL );

    if (    ( sweep_what == VOLTAGE && fabs( *start ) > MAX_SOURCE_LEVELV )
         || ( sweep_what == CURRENT && fabs( *start ) > MAX_SOURCE_LEVELI ) )
    {
        print( FATAL, "Sweep start %s too large.\n", what );
        THROW( EXCEPTION );
    }

    if ( ! ( v = vars_pop( v ) ) )
    {
        print( FATAL, "Missing sweep end %s argument.\n", what );
        THROW( EXCEPTION );
    }

    *end = get_double( v, NULL );
    if (    ( sweep_what == VOLTAGE && fabs( *end ) > MAX_SOURCE_LEVELV )
         || ( sweep_what == CURRENT && fabs( *end ) > MAX_SOURCE_LEVELI ) )
    {
        print( FATAL, "Sweep end %s too large.\n", what );
        THROW( EXCEPTION );
    }

    if ( ! ( v = vars_pop( v ) ) )
    {
        print( FATAL, "Missing argument with number of points of sweep.\n" );
        THROW( EXCEPTION );
    }

    long int np = get_strict_long( v, "number of points in sweep" );

    if ( np < 2 )
    {
        print( FATAL, "Invalid number of points in sweep %ld, must be at "
               "least 2.\n", np );
        THROW( EXCEPTION );
    }

    if ( np >   MAX_SWEEP_RESULT_POINTS
              / ( measure_what != VOLTAGE_AND_CURRENT ? 1 : 2 ) )
    {
        print( FATAL, "Number of points (%ld) of sweep too large, maximum is "
               "%d.\n", np,
                 MAX_SWEEP_RESULT_POINTS
               / ( measure_what != VOLTAGE_AND_CURRENT ? 1 : 2 ) );
        THROW( EXCEPTION );
    }

    *num_points = np;

    too_many_arguments( v );
}


/*--------------------------------------------------------------*
 * Helper function for extracting the arguments to the various
 * list sweep functions
 *--------------------------------------------------------------*/

static
Var_T *
get_list_sweep_params( Var_T        * v,
                       int            sweep_what,
                       int            measure_what,
                       unsigned int * ch )
{
    const char * what = sweep_what == VOLTAGE ? "voltage" : "current";
    const char * What = sweep_what == VOLTAGE ? "Voltage" : "Current";

    *ch = get_channel( &v );

    if ( ! v )
    {
        print( FATAL, "Missing %s sweep list argument.\n", what );
        THROW( EXCEPTION );
    }

    if ( ( v->type != INT_ARR && v->type != FLOAT_ARR ) || v->dim != 1)
    {
        print( FATAL, "%s sweep list argument isn't a 1-dimensional "
               "array.\n", What );
        THROW( EXCEPTION );
    }

    if ( v->len < 2 )
    {
        print( FATAL, "%s sweep list contains less than 2 elements.\n",
               What );
        THROW( EXCEPTION );
    }

    if ( v->len >   MAX_SWEEP_RESULT_POINTS
                  / ( measure_what != VOLTAGE_AND_CURRENT ? 1 : 2 ) )
    {
        print( FATAL, "Number of points (%ld) of sweep list too large, "
               "maximum is %d.\n", v->len,
                 MAX_SWEEP_RESULT_POINTS
               / ( measure_what != VOLTAGE_AND_CURRENT ? 1 : 2 ) );
        THROW( EXCEPTION );
    }

    if ( v->next )
        print( WARN, "Too many arguments, discarding superfluous argument%s.\n",
               v->next->next != NULL ? "s" : "" );

    double max_val = 0;
    for ( ssize_t i = 0; i < v->len; i++ )
        if ( v->type == INT_VAR )
            max_val = d_max( max_val, fabs( ( double ) v->val.lpnt[ i ] ) );
        else
            max_val = d_max( max_val, fabs( v->val.dpnt[ i ] ) );

    if (    ( sweep_what == VOLTAGE && max_val > MAX_SOURCE_LEVELV )
         || ( sweep_what == CURRENT && max_val > MAX_SOURCE_LEVELI ) )
    {
        pp_buf buf;
        print( FATAL, "Largest value of %s in %s sweep list too large.\n",
               sweep_what == VOLTAGE ?
               pp_v( max_val, buf ) : pp_a( max_val, buf ), what );
        THROW( EXCEPTION );
    }

    return v;
}


/*--------------------------------------------------------------*
 * Helper function for checking data returned from sweep 
 *--------------------------------------------------------------*/

static
void
check_sweep_data( const double * data,
                  int            num_points )
{
    for ( int i = 0; i < num_points; i++ )
        if ( fabs( data[ i ] ) >= 9.9e37 )
        {
            print( WARN, "One or more data points from sweep out of range.\n" );
            return;
        }
}


/*--------------------------------------------------------------*
 * Helper function for extracting the channel number from the
 * variables passed to an EDL function
 * 
 * Note: if there's only a single channel and the module was
 * compiled with the macro 'DONT_EXPECT_CHANNEL' being defined
 * none of the EDL functions takes a channel argument (which is
 * a bi redundant when there's only one channel)
 *--------------------------------------------------------------*/

static
#if defined DONT_EXPECT_CHANNEL && NUM_CHANNELS == 1
unsigned int
get_channel( Var_T ** v  UNUSED_ARG )
{
    return 0;
}
#else
unsigned int
get_channel( Var_T ** v )
{
    if ( ! *v )
    {
        print( FATAL, "Missing channel argument.\n" );
        THROW( EXCEPTION );
    }

    long int ch = get_strict_long( *v, "channel number" );

    if ( ch < 1 || ch > NUM_CHANNELS )
    {
        print( FATAL, "Invalid channel number: %ld\n", ch );
        THROW( EXCEPTION );
    }

    *v = vars_pop( *v );
    return ch - 1;
}
#endif

/*--------------------------------------------------------------*
 * When switching to HIGHC mode when a channel is on or swiching
 * the channel on when it's set to HIGHC mode automatically
 * modifies a few settings of the device. To reproduce this
 * behaviour during the test run this function is called.
 *--------------------------------------------------------------*/

static
void
correct_for_highc( unsigned int ch )
{
    if ( ! k26->source[ ch ].highc || !  k26->source[ ch ].output )
        return;

    double low = keithley2612b_min_source_limiti( ch );

    if ( k26->source[ ch ].limiti < low )
        k26->source[ ch ].limiti = low;

    low = keithley2612b_min_source_rangei( ch );
    if ( k26->source[ ch ].rangei < low )
        k26->source[ ch ].rangei = low;

    low = keithley2612b_min_source_lowrangei( ch );
    if ( k26->source[ ch ].lowrangei < low )
        k26->source[ ch ].lowrangei = low;

    low = keithley2612b_min_measure_lowrangei( ch );
    if ( k26->measure[ ch ].lowrangei < low  )
        k26->measure[ ch ].lowrangei = low;
}


/*---------------------------------------------------------------------*
 * Helper function for printing channel numbers
 *---------------------------------------------------------------------*/

static
const char *
ppc( unsigned int   ch,
     const char   * prefix )
{
#if defined DONT_EXPECT_CHANNEL && NUM_CHANNELS == 1
    return " ";
#else
    static char buf[ 50 ] = "for channel * ";

    snprintf( buf, 49, "%s channel %u ", prefix, ch + 1 );
    return buf;
#endif
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static
void
default_settings( void )
{
    for ( unsigned int ch = 0; ch < NUM_CHANNELS; ch++ )
    {
        k26->sense[ ch ] = SENSE_LOCAL;

        k26->source[ ch ].output = false;
        k26->source[ ch ].highc = false;
        k26->source[ ch ].offmode = OUTPUT_NORMAL;
        k26->source[ ch ].func = OUTPUT_DCVOLTS;
        k26->source[ ch ].delay = 0;

        k26->source[ ch ].autorangev = true;
        k26->source[ ch ].autorangei = true;

#if    defined _2601A || defined _2602A \
    || defined _2601B || defined _2602B || defined _2604B
        k26->source[ ch ].limitv = 40;
        k26->source[ ch ].limiti = 1;
#else
        k26->source[ ch ].limitv = 20;
        k26->source[ ch ].limiti = 100.0e-3;
#endif

#if    defined _2601A || defined _2602A || defined _2611A || defined _2612A \
    || defined _2601B || defined _2602B || defined _2604B
        k26->source[ ch ].rangev = 100.0e-3;
#else
        k26->source[ ch ].rangev = 200.0e-3;
#endif

#if    defined _2601A || defined _2602A || defined _2611A || defined _2612A \
    || defined _2601B || defined _2602B || defined _2604B                   \
    || defined _2611B || defined _2612B || defined _2614B
        k26->source[ ch ].rangei = 100.0e-9;
#else
        k26->source[ ch ].rangei = 1.0e-9;
#endif

#if    defined _2601A || defined _2602A                   \
    || defined _2601B || defined _2602B || defined _2604B
        k26->source[ ch ].lowrangev = 100.0e-3;
#else
        k26->source[ ch ].lowrangev = 200.0e-3;
#endif

#if    defined _2601A || defined _2602A || defined _2611A || defined _2612A \
    || defined _2601B || defined _2602B || defined _2604B                   \
    || defined _2611B || defined _2612B || defined _2614B
        k26->source[ ch ].lowrangei = 100.0e-9;
#else
        k26->source[ ch ].lowrangei = 1.0e-9;
#endif

        k26->source[ ch ].levelv = 0;
        k26->source[ ch ].leveli = 0;

        k26->source[ ch ].offlimiti = 1.0e-3;

        k26->source[ ch ].settling = SETTLE_SMOOTH;

        k26->source[ ch ].sink = false;

        k26->measure[ ch ].autorangev = true;
        k26->measure[ ch ].autorangei = true;

        k26->measure[ ch ].autozero = AUTOZERO_AUTO;

#if defined _2601A || defined _2602A || defined _2611A || defined _2612A
        k26->measure[ ch ].filter.type = FILTER_MOVING_AVG;
#else
        k26->measure[ ch ].filter.type = FILTER_REPEAT_AVG;
#endif

        k26->measure[ ch ].filter.enabled = false;
        k26->measure[ ch ].filter.count = false;

        k26->measure[ ch ].time = 1.0 / k26->linefreq;

#if    defined _2601A || defined _2602A || defined _2611A || defined _2612A \
    || defined _2601B || defined _2602B || defined _2604B
        k26->measure[ ch ].delay = DELAY_OFF;
#else
        k26->measure[ ch ].delay = DELAY_AUTO;
#endif

        k26->measure[ ch ].relv.level = 0;
        k26->measure[ ch ].relv.enabled = false;

        k26->measure[ ch ].reli.level = 0;
        k26->measure[ ch ].reli.enabled = false;

        k26->contact[ ch ].speed = CONTACT_FAST;
        k26->contact[ ch ].threshold = 50;
    }
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
