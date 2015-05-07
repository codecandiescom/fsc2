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


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

static unsigned int get_channel( Var_T ** v );
static void correct_for_highc( unsigned int ch );
static char * pretty_print( double v,
                            char   unit );
#define ppA( x ) pretty_print( x, 'A' )
#define ppV( x ) pretty_print( x, 'V' )


static Keithley2600A_T keithley2600a;
static Keithley2600A_T keithley2600a_test;
Keithley2600A_T * k26 = &keithley2600a;


/*--------------------------------------------------------------*
 * Init hook
 *--------------------------------------------------------------*/

int
keithley2600a_init_hook( void )
{
    unsigned int ch;

    k26 = &keithley2600a;

    k26->is_open = false;
    k26->comm_failed = false;

    for ( ch = 0; ch < NUM_CHANNELS; ch++ )
        k26->keep_on_at_end[ ch ] = false;

    return 1;
}


/*--------------------------------------------------------------*
 * Start of test hook
 *--------------------------------------------------------------*/

int
keithley2600a_test_hook( void )
{
    int ch;

    keithley2600a_test = keithley2600a;
    k26 = &keithley2600a_test;

    /* Set up the structures for both channels with the device's default
       settings */

    for ( ch = 0; ch < NUM_CHANNELS; ch++ )
    {
        k26->sense[ ch ]         = SENSE_LOCAL;

        k26->source[ ch ].output  = OUTPUT_OFF;
        k26->source[ ch ].offmode = OUTPUT_NORMAL;
        k26->source[ ch ].highc   = UNSET;
        k26->source[ ch ].func    = OUTPUT_DCVOLTS;

        k26->source[ ch ].autorangev = UNSET;
        k26->source[ ch ].autorangei = UNSET;

        k26->source[ ch ].rangev     = 0.2;
        k26->source[ ch ].rangei     = 1.0e-7;

        k26->source[ ch ].lowrangev  = 0.2;
        k26->source[ ch ].lowrangei  = 1.0e-7;

        k26->source[ ch ].levelv     = 0;
        k26->source[ ch ].leveli     = 0;

        k26->source[ ch ].limitv     = 20.0;
        k26->source[ ch ].limiti     = 0.1;


        k26->measure[ ch ].autorangev = SET;
        k26->measure[ ch ].autorangei = SET;

        k26->measure[ ch ].rangev     = 0.2;
        k26->measure[ ch ].rangei     = 0.1;

        k26->measure[ ch ].lowrangev  = 0.2;
        k26->measure[ ch ].lowrangei  = 1.0e-7;

        k26->measure[ ch ].autozero   = AUTOZERO_AUTO;
    }
 
    return OK;
}


/*--------------------------------------------------------------*
 * Start of experiment hook
 *--------------------------------------------------------------*/

int
keithley2600a_exp_hook( void )
{
    k26 = &keithley2600a;

    if ( ! keithley2600a_open( ) )
        return FAIL;

    TRY
    {
        keithley2600a_get_state( );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        keithley2600a_close( );
        RETHROW;
    }

    return OK;
}
   

/*--------------------------------------------------------------*
 * End of experiment hook
 *--------------------------------------------------------------*/

int
keithley2600a_end_of_exp_hook( void )
{
    unsigned int ch;

    /* If there was no communication failure switch off all channels
       unless the user explicitely asked for them to be left on. On
       communication failures we better don't try any further communication,
       so we have to leave the channels on but warn the user about it */

    if ( ! k26->comm_failed )
        for ( ch = 0; ch < NUM_CHANNELS; ch++ )
            if ( k26->source[ ch ].output && ! k26->keep_on_at_end[ ch ] )
                keithley2600a_set_source_output( ch, OUTPUT_OFF );
    else
        for ( ch = 0; ch < NUM_CHANNELS; ch++ )
            if ( k26->source[ ch ].output )
            {
                print( WARN, "Please be careful: can't switch off output of "
                       "device!\n" );
                break;
            }

    /* Close connection to the device */

    return keithley2600a_close( );
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
 * Returns or sets the sense mode, i.e. 2- or 4-wire measurements
 * (or calibration)
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_sense_mode( Var_T * v )
{
    unsigned int ch = get_channel( &v );
    long int sense;

    if ( ! v )
        return vars_push( INT_VAR, ( long ) k26->sense[ ch ] );

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
        keithley2600a_set_sense( ch, sense );
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
    long int mode;

    if ( ! v )
        return vars_push( INT_VAR, ( long ) k26->source[ ch ].offmode );

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
        keithley2600a_set_source_offmode( ch, mode );
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

    /* Before switching the channel on check that all settings are possible */

    if ( on_off )
    {
        /* Tests require that channel looks as if it is on */

        k26->source[ ch ].output = true;

        if ( k26->source[ ch ].func == OUTPUT_DCAMPS )
        {
            if (    ! keithley2600a_test_source_leveli( ch )
                 || ! keithley2600a_test_source_rangei( ch )
                 || ! keithley2600a_test_source_limitv( ch ) )
            {
                char * s1 = ppA( k26->source[ ch ].leveli );
                char * s2 = ppA( k26->source[ ch ].rangei );
                char * s3 = ppV( k26->source[ ch ].levelv );

                print( FATAL, "Can't switch on channel %u as current source, "
                       "combination of current level (%s), range (%s) "
                       "or compliance voltage (%s) is not possible.\n",
                       ch, s1, s2, s3 );
                T_free( s3 );
                T_free( s2 );
                T_free( s1 );
                k26->source[ ch ].output = false;
                THROW( EXCEPTION );
            }
        }
        else
        {
            if (    ! keithley2600a_test_source_levelv( ch )
                 || ! keithley2600a_test_source_rangev( ch )
                 || ! keithley2600a_test_source_limiti( ch ) )
            {
                char * s1 = ppV( k26->source[ ch ].levelv );
                char * s2 = ppV( k26->source[ ch ].rangev );
                char * s3 = ppA( k26->source[ ch ].leveli );

                print( FATAL, "Can't switch on channel %u as voltage source, "
                       "combination of voltage level (%s), range (%s) or "
                       "compliance current (%s)is not possible.\n",
                       s1, s2, s3 );
                T_free( s3 );
                T_free( s2 );
                T_free( s1 );
                k26->source[ ch ].output = false;
                THROW( EXCEPTION );
            }
        }

        k26->source[ ch ].output = false;
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_output( ch, on_off );
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
    int mode;

    if ( ! v )
        return vars_push( INT_VAR, k26->source[ ch ].func == OUTPUT_DCAMPS ?
                          0L : 1L );

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
            print( FATAL, "Invalid argument: \"%s\".\n" );
            THROW( EXCEPTION );
        }
    }
    else
        mode = get_strict_long( v, "spurce mode" ) != 0;

    if ( keithley2600a.source[ ch ].func == mode )
        return vars_push( INT_VAR, mode == OUTPUT_DCAMPS ? 0L : 1L );

    too_many_arguments( v );

    if ( mode == k26->source[ ch ].func )
        return vars_push( INT_VAR, mode );

    /* before switching the source function when output is on we need to
       check that all settings for the new mode are correct */

    if ( k26->source[ ch ].output )
    {
        int old_mode = k26->source[ ch ].func;

        /* For testing the mode must already be set! */

        k26->source[ ch ].func = mode;

        if ( mode == OUTPUT_DCAMPS )
        {
            if (    ! keithley2600a_test_source_leveli( ch )
                 || ! keithley2600a_test_source_rangei( ch )
                 || ! keithley2600a_test_source_limitv( ch ) )
            {
                char * s1 = ppA( k26->source[ ch ].leveli );
                char * s2 = ppA( k26->source[ ch ].rangei );
                char * s3 = ppV( k26->source[ ch ].levelv );

                print( FATAL, "Can't switch to current sourrce mode, "
                       "combination of current level (%s), range (%s) "
                       "or compliance voltage (%s) is not possible.\n",
                       s1, s2, s3 );
                T_free( s3 );
                T_free( s2 );
                T_free( s1 );
                k26->source[ ch ].func = old_mode;
                THROW( EXCEPTION );
            }
        }
        else
        {
            if (    ! keithley2600a_test_source_levelv( ch )
                 || ! keithley2600a_test_source_rangev( ch )
                 || ! keithley2600a_test_source_limiti( ch ) )
            {
                char * s1 = ppV( k26->source[ ch ].levelv );
                char * s2 = ppV( k26->source[ ch ].rangev );
                char * s3 = ppA( k26->source[ ch ].leveli );

                print( FATAL, "Can't switch to current sourrce mode, "
                       "combination of voltage level (%s), range (%s) or "
                       "compliance current (%s)is not possible.\n",
                       s1, s2, s3 );
                T_free( s3 );
                T_free( s2 );
                T_free( s1 );
                k26->source[ ch ].func = old_mode;
                THROW( EXCEPTION );
            }
        }

        k26->source[ ch ].func = old_mode;
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_func( ch, mode );
    else
        keithley2600a.source[ ch ].func = mode;

    return vars_push( INT_VAR, k26->source[ ch ].func == OUTPUT_DCAMPS ?
                      0L : 1L );
}


/*--------------------------------------------------------------*
 * Returns source voltage or sets it for the given channel
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_voltage( Var_T * v )
{
    unsigned int ch = get_channel( &v );
    double volts;

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].levelv );

    volts = get_double( v, "source voltage" );
    too_many_arguments( v );

    if ( ! keithley2600a_check_source_levelv( ch, volts ) )
    {
        char * s1 = ppV( volts );
        char * s2 = ppV( keithley2600a_max_source_levelv( ch ) );

        print( FATAL, "Source voltage of %s is out of currently possible "
               "range, may not exceed +/-%s.\n", s1, s2 );
        T_free( s2 );
        T_free( s1 );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_levelv( ch, volts );
    else
    {
        k26->source[ ch ].levelv = volts;
        if ( k26->source[ ch ].autorangev )
            k26->source[ ch ].rangev =
                                 keithley2600a_best_source_rangev( ch, volts );
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
    double amps;

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].leveli );

    amps = get_double( v, "source current" );
    too_many_arguments( v );

    if ( ! keithley2600a_check_source_leveli( ch, amps ) )
    {
        char * s1 = ppA( amps );
        char * s2 = ppA( keithley2600a_max_source_leveli( ch ) );

        print( FATAL, "Source current of %s is out of currently possible "
               "range, may not exceed +/-%s.\n", s1, s2 );
        T_free( s2 );
        T_free( s1 );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_leveli( ch, amps );
    else
    {
        k26->source[ ch ].leveli = amps;
        if ( k26->source[ ch ].autorangei )
            k26->source[ ch ].rangei =
                                  keithley2600a_best_source_rangei( ch, amps );
    }

    return vars_push( FLOAT_VAR, k26->source[ ch ].leveli );
}


/*--------------------------------------------------------------*
 * Returns source voltage range or sets it for the given channel
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_voltage_range( Var_T * v )
{
    unsigned int ch = get_channel( &v );
    double range,
           req_range;

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].rangev );

    req_range = fabs( get_double( v, "source voltage range" ) );
    too_many_arguments( v );

    range = keithley2600a_best_source_rangev( ch, req_range );

    if ( range < 0 )
    {
        char * s1 = ppV( req_range );
        char * s2 = ppV( keithley2600a_max_source_rangev( ch ) );

        print( FATAL, "Can't set source voltage range to %s, maximum possible "
               "value under the current circumstances is %s.\n", s1, s2 );
        T_free( s2 );
        T_free( s1 );
        THROW( EXCEPTION );
    }

    if ( fabs( ( range - req_range ) / range ) > 0.001 )
    {
        char * s1 = ppV( req_range );
        char * s2 = ppV( range );

        print( WARN, "Had to adjust source voltage range from %s to %s.\n",
               s1, s2 );
        T_free( s2 );
        T_free( s1 );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_rangev( ch, range );
    else
    {
        k26->source[ ch ].rangev = range;
        k26->source[ ch ].autorangev = false;
    }

    return vars_push( FLOAT_VAR, range );
}


/*--------------------------------------------------------------*
 * Returns source current range or sets it for the given channel
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_current_range( Var_T * v )
{
    unsigned int ch = get_channel( &v );
    double range,
           req_range;

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].rangei );

    req_range = fabs( get_double( v, "current voltage range" ) );
    too_many_arguments( v );

    range = keithley2600a_best_source_rangei( ch, req_range );

    if ( range < 0 )
    {
        char * s1 = ppV( req_range );
        char * s2 = ppV( keithley2600a_max_source_rangei( ch ) );

        print( FATAL, "Can't set source current range to %s, maximum possible "
               "value under the current circumstances is %s.\n", s1, s2 );
        T_free( s2 );
        T_free( s1 );
        THROW( EXCEPTION );
    }

    if ( fabs( ( range - req_range ) / range ) > 0.001 )
    {
        char * s1 = ppA( req_range );
        char * s2 = ppA( range );

        print( WARN, "Had to adjust source current range from %s to %s.\n",
               s1, s2 );
        T_free( s2 );
        T_free( s1 );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_rangei( ch, range );
    else
    {
        k26->source[ ch ].rangei = range;
        k26->source[ ch ].autorangei = false;
    }

    return vars_push( FLOAT_VAR, range );
}


/*--------------------------------------------------------------*
 * Returns or sets if source voltage autoranging is on or off
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_voltage_autoranging( Var_T * v )
{
    unsigned int ch = get_channel( &v );
    bool on_off;

    if ( ! v )
        return vars_push( INT_VAR, k26->source[ ch ].autorangev ? 1L : 0L );

    on_off = get_boolean( v );
    too_many_arguments( v );

    if ( on_off == k26->source[ ch ].autorangev )
        return vars_push( INT_VAR, on_off ? 1L : 0L );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_autorangev( ch, on_off );
    else
    {
        /* Keep in mind that switching autoranging on may change the range
           setting */

        k26->source[ ch ].autorangev = on_off;
        if ( on_off )
            k26->source[ ch ].rangev =
                  keithley2600a_best_source_rangev( ch,
                                                    k26->source[ ch ].levelv );
    }

    return vars_push( INT_VAR, k26->source[ ch ].autorangev ? 1L : 0L );
}


/*--------------------------------------------------------------*
 * Returns or sets the source voltage autoranging low limit
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_voltage_autorange_low_limit( Var_T * v )
{
    unsigned int ch = get_channel( &v );
    double req_range,
           range;

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].lowrangev );

    req_range = fabs( get_double( v, "source voltage autorange low limit" ) );
    too_many_arguments( v );

    /* Find nearest possible range */

    range = keithley2600a_best_source_rangev( ch, req_range );

    /* A negative result indicates that the value was too large */

    if ( range < 0 )
    {
        char * s1 = ppV( req_range );
        char * s2 = ppV( keithley2600a_max_source_rangev( ch ) );

        print( FATAL, "Can't set source voltage autorange low limit to %s, "
               "maximum possible value under the current circumstances "
               "is %s.\n", s1, s2 );
        T_free( s2 );
        T_free( s1 );
        THROW( EXCEPTION );
    }

    /* Warn if we had to modify the requested value */

    if ( fabs( ( range - req_range ) / range ) > 0.001 )
    {
        char * s1 = ppV( req_range );
        char * s2 = ppV( range );

        print( WARN, "Had to adjust source voltage autorange low limit "
               "from %s to %s.\n", s1, s2 );
        T_free( s2 );
        T_free( s1 );
    }

    /* Check that the value is reasonable */

    if ( ! keithley2600a_check_source_lowrangev( ch, range ) )
    {
        char * s1 = ppV( range );
        char * s2 = ppV( keithley2600a_min_measure_lowrangev( ch ) );
        char * s3 = ppV( keithley2600a_max_source_rangev( ch ) );

        print( FATAL, "Requested source voltage autorange low limit of %s "
               "out of range, must be betwen %s and %s under current "
               "circumstances.\n", s1, s2, s3 );
        T_free( s1 );
        T_free( s2 );
        T_free( s3 );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_lowrangev( ch, range );
    else
    {
        k26->source[ ch ].lowrangev = range;
        if ( k26->source[ ch ].autorangev )
            k26->source[ ch ].rangev = d_max( range, k26->source[ ch ].rangev );
    }

    return vars_push( FLOAT_VAR, k26->source[ ch ].lowrangev );
}


/*--------------------------------------------------------------*
 * Returns or sets the source current autoranging low limit
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_current_autorange_low_limit( Var_T * v )
{
    unsigned int ch = get_channel( &v );
    double req_range,
           range;

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].lowrangei );

    req_range = fabs( get_double( v, "source current autorange low limit" ) );
    too_many_arguments( v );

    /* Find nearest possible range */

    range = keithley2600a_best_source_rangei( ch, req_range );

    /* A negative result indicates that the value was too large */

    if ( range < 0 )
    {
        char * s1 = ppA( req_range );
        char * s2 = ppA( keithley2600a_max_source_rangei( ch ) );

        print( FATAL, "Can't set source current autorange low limit to %s, "
               "maximum possible value under the current circumstances "
               "is %s.\n", s1, s2 );
        T_free( s2 );
        T_free( s1 );
        THROW( EXCEPTION );
    }

    /* Warn if we had to modify the requested value */

    if ( fabs( ( range - req_range ) / range ) > 0.001 )
    {
        char * s1 = ppA( req_range );
        char * s2 = ppA( range );

        print( WARN, "Had to adjust source current autorange low limit "
               "from %s to %s.\n", s1, s2 );
        T_free( s2 );
        T_free( s1 );
    }

    /* Check that the value is reasonable */

    if ( ! keithley2600a_check_source_lowrangei( ch, range ) )
    {
        char * s1 = ppA( range );
        char * s2 = ppA( keithley2600a_min_measure_lowrangei( ch ) );
        char * s3 = ppA( keithley2600a_max_source_rangei( ch ) );

        print( FATAL, "Requested source current autorange low limit of %s "
               "out of range, must be betwen %s and %s under current "
               "circumstances.\n", s1, s2, s3 );
        T_free( s1 );
        T_free( s2 );
        T_free( s3 );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_lowrangei( ch, range );
    else
    {
        k26->source[ ch ].lowrangei = range;
        if ( k26->source[ ch ].autorangei )
            k26->source[ ch ].rangei = d_max( range, k26->source[ ch ].rangei );
    }

    return vars_push( FLOAT_VAR, k26->source[ ch ].lowrangei );
}


/*--------------------------------------------------------------*
 * Returns or sets if source current autoranging is on or off
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_current_autoranging( Var_T * v )
{
    unsigned int ch = get_channel( &v );
    bool on_off;

    if ( ! v )
        return vars_push( INT_VAR, k26->source[ ch ].autorangei ? 1L : 0L );

    on_off = get_boolean( v );
    too_many_arguments( v );

    if ( on_off == k26->source[ ch ].autorangei )
        return vars_push( INT_VAR, on_off ? 1L : 0L );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_autorangei( ch, on_off );
    else
    {
        /* Keep in mind that switching autoranging on may change the range
           setting */

        k26->source[ ch ].autorangei = on_off;
        if ( on_off )
            k26->source[ ch ].rangei =
                  keithley2600a_best_source_rangei( ch,
                                                    k26->source[ ch ].leveli );
    }

    return vars_push( INT_VAR, k26->source[ ch ].autorangev ? 1L : 0L );
}


/*--------------------------------------------------------------*
 * Returns or sets the voltage compliance limit
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_compliance_voltage( Var_T * v )
{
    unsigned int ch = get_channel( &v );
    double limit;

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].limitv );

    /* Accept negative values, jut make 'em positive */

    limit = fabs( get_double( v, "compliance voltage" ) );
    too_many_arguments( v );
    
    if ( limit == k26->source[ ch ].limitv )
        return vars_push( FLOAT_VAR, limit );

    if ( ! keithley2600a_check_source_limitv( ch, limit ) )
    {
        char * s1 = ppV( limit );
        char * s2 = ppV( keithley2600a_min_source_limitv( ch ) );
        char * s3 = ppV( keithley2600a_max_source_limitv( ch ) );

        print( FATAL, "Requested compliance voltage of %s is out of range, "
               "must be between %s and %s und the current circumstances.\n",
               s1, s2, s3 );
        T_free( s1 );
        T_free( s2 );
        T_free( s3 );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_limitv( ch, limit );
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
    double limit;

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].limiti );

    /* Accept negative values, jut make 'em positive */

    limit = fabs( get_double( v, "compliance current" ) );
    too_many_arguments( v );
    
    if ( limit == k26->source[ ch ].limiti )
        return vars_push( FLOAT_VAR, limit );

    if ( ! keithley2600a_check_source_limiti( ch, limit ) )
    {
        char * s1 = ppA( limit );
        char * s2 = ppA( keithley2600a_min_source_limiti( ch ) );
        char * s3 = ppA( keithley2600a_max_source_limiti( ch ) );

        print( FATAL, "Requested compliance current of %s is out of range, "
               "must be between %s and %s und the current circumstances.\n",
               s1, s2, s3 );
        T_free( s1 );
        T_free( s2 );
        T_free( s3 );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_limiti( ch, limit );
    else
        k26->source[ ch ].limiti = limit;

    return vars_push( FLOAT_VAR, k26->source[ ch ].limiti );
}


/*--------------------------------------------------------------*
 * Returns or sets the source delay
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_source_delay( Var_T * v )
{
    unsigned int ch = get_channel( &v );
    double delay;

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].delay );

    delay = get_double( v, "source delay" );

    if ( delay < 0 )
        delay = DELAY_AUTO;

    if ( delay == k26->source[ ch ].delay )
        vars_push( FLOAT_VAR, k26->source[ ch ].delay );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_delay( ch, delay );
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
            keithley2600a_set_source_highc( ch, highc );
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
    bool sink;

    if ( ! v )
        return vars_push( INT_VAR, k26->source[ ch ].sink ? 1L : 0L );

    sink = get_boolean( v );
    too_many_arguments( v );

    if ( sink == k26->source[ ch ].sink )
        return vars_push( INT_VAR, sink ? 1L : 0L );

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_sink( ch, sink );
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
    long int settle;

    if ( ! v )
        return vars_push( INT_VAR, ( long ) k26->source[ ch ].settling );

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
        keithley2600a_set_source_settling( ch,settle );
    else
        k26->source[ ch ].settling = settle;

    return vars_push( INT_VAR, ( long ) k26->source[ ch ].settling );
}


/*--------------------------------------------------------------*
 * Returns or sets the maximum source current when in normal off state
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_max_off_source_current( Var_T * v )
{
    unsigned int ch = get_channel( &v );
    double limit;

    if ( ! v )
        return vars_push( FLOAT_VAR, k26->source[ ch ].offlimiti );

    limit = fabs( get_double( v, "maximum current in normal off state" ) );
    too_many_arguments( v );

    if ( limit == k26->source[ ch ].offlimiti )
         return vars_push( FLOAT_VAR, limit );

    if ( ! keithley2600a_check_source_offlimiti( ch, limit ) )
    {
        char * s1 = ppA( limit );
        char * s2 = ppA( keithley2600a_min_source_offlimiti( ch ) );
        char * s3 = ppA( keithley2600a_max_source_offlimiti( ch ) );

        print( FATAL, "Requesyed maximum current in normal off state of %s "
               "out of range, must be between %s and %s.\n", s1, s2, s3 );
        T_free( s1 );
        T_free( s2 );
        T_free( s3 );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_offlimiti( ch, limit );
    else
        k26->source[ ch ].offlimiti = limit;

    return vars_push( FLOAT_VAR, k26->source[ ch ].offlimiti );
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
    long ch;

    if ( ! *v )
    {
        print( FATAL, "Missing channel argument.\n" );
        THROW( EXCEPTION );
    }

    ch = get_strict_long( *v, "channel number" );

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
    double low;

    if ( ! k26->source[ ch ].highc || !  k26->source[ ch ].output )
        return;

    low = keithley2600a_min_source_limiti( ch );

    if ( k26->source[ ch ].limiti < low )
        k26->source[ ch ].limiti = low;

    low = keithley2600a_min_source_rangei( ch );
    if ( k26->source[ ch ].rangei < low )
        k26->source[ ch ].rangei = low;

    low = keithley2600a_min_source_lowrangei( ch );
    if ( k26->source[ ch ].lowrangei < low )
        k26->source[ ch ].lowrangei = low;

    low = keithley2600a_min_measure_lowrangei( ch );
    if ( k26->measure[ ch ].lowrangei < low  )
        k26->measure[ ch ].lowrangei = low;
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static
char *
pretty_print( double v,
              char   unit )
{
    double av = fabs( v );

    if ( av >= 1.0 )
        return get_string( "%.5g %c", v, unit );
    else if ( av >= 1.0e-3 )
        return get_string( "%.5g m%c", v * 1.0e3, unit );
    else if ( av >= 1.0e-6 )
        return get_string( "%.5g u%c", v * 1.0e6, unit );
    else if ( av >= 1.0e-9 )
        return get_string(  "%.5g n%c", v * 1.0e9, unit );

    return get_string( "%.5g p%c", v * 1.0e12, unit );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
