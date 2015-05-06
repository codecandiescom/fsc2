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
 * Start of experiment hook
 *--------------------------------------------------------------*/

int
keithley2600a_test_hook( void )
{
    int ch;

    k26 = &keithley2600a_test;

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
 * returns source output state for the given channel or switches
 * it on or off
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_output_state( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( v )
    {
        bool on_off = get_boolean( v );

        if ( k26->source[ ch ].output == on_off )
            return vars_push( INT_VAR, on_off ? 1L : 0L );

        if ( FSC2_MODE == EXPERIMENT )
            keithley2600a_set_source_output( ch, on_off );
        else
            k26->source[ ch ].output = on_off;
    }

    return vars_push( INT_VAR, k26->source[ ch ].output ? 1L : 0L );
}


/*--------------------------------------------------------------*
 * Returns or sets the source mode for the fivven channel
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

    if ( ! keithley2600a_check_source_levelv( volts, ch ) )
    {
        print( FATAL, "Source voltage of %.5g V is out of currently possible "
               "range, may not be outside +/-%g V.\n", volts,
               keithley2600a_max_source_levelv( ch ) );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        keithley2600a_set_source_levelv( ch, volts );
    else
        k26->source[ ch ].levelv = volts;

    return vars_push( FLOAT_VAR, k26->source[ ch ].levelv );
}


/*--------------------------------------------------------------*
 * Returns source high capacity state for the given channel
 *  or switches it on or off
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_high_capacity( Var_T * v )
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
        print( FATAL, "Invalid channel number %ld\n", ch );
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
    if ( v >= 1.0 )
        return get_string( "%.3f %c", v, unit );
    else if ( v >= 1.0e-3 )
        return get_string( "%.3f m%c", v * 1.0e3, unit );
    else if ( v >= 1.0e-6 )
        return get_string( "%.3f u%c", v * 1.0e6, unit );
    else if ( v >= 1.0e-9 )
        return get_string(  "%.3f n%c", v * 1.0e9, unit );

    return get_string( "%.3f p%c", v * 1.0e12, unit );
}

/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
