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


static Keithley2600A_T keithley2600a;
static Keithley2600A_T keithley2600a_test;
Keithley2600A_T * k26 = &keithley2600a;


/*--------------------------------------------------------------*
 * Start of experiment hook
 *--------------------------------------------------------------*/

int
keithley2600a_test_hook( void )
{
    k26 = &keithley2600a_test;

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

    keithley2600a_get_state( );

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
 * Switches output for the selected channel on or off
 *--------------------------------------------------------------*/

Var_T *
sourcemeter_output( Var_T * v )
{
    unsigned int ch = get_channel( &v );

    if ( v )
    {
        bool on_off = get_boolean( v );

        if ( FSC2_MODE == EXPERIMENT )
            keithley2600a_set_source_output( ch, on_off );
        else
            k26->source.output[ ch ] = on_off;
    }

    return vars_push( INT_VAR, k26->source.output[ ch ] ? 1L : 0L );
}



/*--------------------------------------------------------------*
 * Helper function for extracting the channel number from the
 * variables passed to an EDL function
 *--------------------------------------------------------------*/

static
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



/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
