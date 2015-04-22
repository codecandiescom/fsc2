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


#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "oriel_cs_130.conf"

#if NUM_GRATINGS < 1 || NUM_GRATINGS > 9
#error "Number of gratings in configration must be between 1 and 9"
#endif


/*--------------------------------*
 * Global variables of the module
 *--------------------------------*/

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


typedef struct {
    int          device;
    int          cur_grating;                    /* 0: grating #1, etc. */

    double       wavelength;                     /* in m */
    double       max_test_wavelength[ NUM_GRATINGS ];

    unsigned int step;
    unsigned int max_step;

    unsigned int num_lines[ NUM_GRATINGS ];      /* in lines per mm */
    double       factor[ NUM_GRATINGS ];
    double       offset[ NUM_GRATINGS ];
    double       zero[ NUM_GRATINGS ];

    bool         shutter_state;                  /* true if shutter closed */

    int          filter;                         /* 0: filter #1, etc. */

    int          gpib_address;
} Oriel_CS_130;


static Oriel_CS_130 oriel_cs_130;


#define MAX_FILTER    6

#define MAX_STEP       1000000     /* XXXXX We need to find out what this is! */



#define TEST_WAVELENGTH      5.5e-7
#define TEST_STEP            100
#define TEST_GRATING_NUMBER  0
#define TEST_SHUTTER_STATE   0
#define TEST_FILTER          0

int oriel_cs_130_init_hook( void );
int oriel_cs_130_exp_hook( void );
 

Var_T * monochromator_name( Var_T * v  UNUSED_ARG );
Var_T * monochromator_grating( Var_T * v );
Var_T * monochromator_wavelength( Var_T * v );
Var_T * monochromator_wavenumber( Var_T * v );
Var_T * monochromator_step_position( Var_T * v );
Var_T * monochromator_groove_density( Var_T * v );
Var_T * monochromator_calibrate( Var_T *v );
Var_T * monochromator_calibration_factor( Var_T * v );
Var_T * monochromator_calibration_offset( Var_T * v );
Var_T * monochromator_shutter( Var_T * v );
Var_T * monochromator_filter( Var_T * v );
Var_T * monochromator_grating_zero( Var_T * v );
Var_T * monochromator_set_gpib_address( Var_T * v );


static bool oriel_cs_130_init( const char * name );
static int oriel_cs_130_get_grating( void );
static int oriel_cs_130_set_grating( int grating );
static unsigned int oriel_cs_130_get_number_of_lines( int grating );
static unsigned int oriel_cs_130_set_number_of_lines( int          grating,
                                                      unsigned int num_lines );
static void oriel_cs_130_calibrate( double wl );
static double oriel_cs_130_get_calibration_factor( int grating );
static double oriel_cs_130_set_calibration_factor( int    grating,
                                                   double factor );
static double oriel_cs_130_get_calibration_offset( int grating );
static double oriel_cs_130_set_calibration_offset( int    grating,
                                                   double offset );
static double oriel_cs_130_get_zero( int    gratings );
static void oriel_cs_130_set_zero( int    gratings,
                                   double zero );
static double oriel_cs_130_get_wavelength( void );
static double oriel_cs_130_set_wavelength( double wl );
static unsigned int oriel_cs_130_get_step( void );
static unsigned int oriel_cs_130_set_step( unsigned int step );
static bool oriel_cs_130_get_shutter( void );
static bool oriel_cs_130_set_shutter( bool on_off );
#if defined HAS_FILTER_WHEEL
static int oriel_cs_130_get_filter( void );
static int oriel_cs_130_set_filter( int filter );
#endif
static int oriel_cs_130_get_error( void );
static void oriel_cs_130_command( const char * cmd );
static long oriel_cs_130_talk( const char * cmd,
                               char       * reply,
                               long         length );
static void oriel_cs_130_failure( void );
static int oriel_cs_130_get_gpib_address( void );
static void oriel_cs_130_set_gpib_address( unsigned int addr );
static double oriel_cs_130_wl2wn( double wl );
static double oriel_cs_130_wn2wl( double wn );


/*------------------------------------*
 * Init hook function for the module.
 *------------------------------------*/

int
oriel_cs_130_init_hook( void )
{
    int i;


    /* Set global variable to indicate that GPIB bus is needed */

    Need_GPIB = SET;

    /* Reset several variables in the structure describing the device */

    oriel_cs_130.device        = -1;

    oriel_cs_130.wavelength = TEST_WAVELENGTH;
    oriel_cs_130.step = TEST_STEP;

    oriel_cs_130.cur_grating = TEST_GRATING_NUMBER;

    for ( i = 0; i < NUM_GRATINGS; i++ )
    {
        oriel_cs_130.max_test_wavelength[ i ] = 0;
        oriel_cs_130.num_lines[ i ] = 600;
        oriel_cs_130.factor[ i ] = 1;
        oriel_cs_130.offset[ i ] = 0.025;
        oriel_cs_130.zero[ i ] = i * 2 * M_PI / NUM_GRATINGS + 0.0872665;
    }

    oriel_cs_130.shutter_state = TEST_SHUTTER_STATE;

    oriel_cs_130.filter = TEST_FILTER;

    oriel_cs_130.gpib_address = -1;

    return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int
oriel_cs_130_exp_hook( void )
{
    /* Initialize the device */

    if ( oriel_cs_130_init( DEVICE_NAME ) )
    {
        print( FATAL, "Initialization of device failed: %s.\n",
               gpib_last_error( ) );
        THROW( EXCEPTION );
    }

    return 1;
}

/*--------------------------------------------------------*
 * Returns the device name
 *--------------------------------------------------------*/

Var_T *
monochromator_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*-------------------------------------------------------------------*
 * Allows to query the currently used grating or switch between them
 *-------------------------------------------------------------------*/

Var_T *
monochromator_grating( Var_T * v )
{
    if ( v )
    {
        long int grating = get_strict_long( v, "grating number" );

        too_many_arguments( v );

        if ( grating < 1 || grating > NUM_GRATINGS )
        {
            print( FATAL, "Invalid grating number %ld, must be between 1 and "
                   "%d.\n", grating + 1, NUM_GRATINGS );
            THROW( EXCEPTION );
        }

        if ( FSC2_MODE == EXPERIMENT )
            oriel_cs_130_set_grating( grating - 1 );
        else
            oriel_cs_130.cur_grating = grating - 1;
    }

    return vars_push( INT_VAR, oriel_cs_130.cur_grating + 1L );
}


/*--------------------------------------------------------*
 * Queries or sets the wavelength the current grating is set to
 *--------------------------------------------------------*/

Var_T *
monochromator_wavelength( Var_T * v )
{
    double wl;
    int grating = oriel_cs_130.cur_grating;

    if ( ! v )
        return vars_push( FLOAT_VAR, oriel_cs_130.wavelength );

    wl = get_double( v, "wavelength" );
    too_many_arguments( v );

    if ( wl == oriel_cs_130.wavelength )
        return vars_push( FLOAT_VAR, oriel_cs_130.wavelength );

    if ( wl < 0 )
    {
        print( FATAL, "Invaid negative wavelength.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
    {
        double max_wl = 4.0 * oriel_cs_130.num_lines[ grating ] / 3;

        if ( wl > max_wl )
        {
            print( FATAL, "Requested Wavelength of %.3f nm too large, maximum "
                   "for grating %d is %.3f nm.\n", wl * 1.0e9, grating + 1,
                   max_wl * 1.0e9 );
            THROW( EXCEPTION );
        }

        oriel_cs_130_set_wavelength( wl );
    }
    else
    {
        oriel_cs_130.wavelength = wl;
        oriel_cs_130.max_test_wavelength[ grating ] =
                       d_max( oriel_cs_130.max_test_wavelength[ grating ], wl );
    }

    return vars_push( FLOAT_VAR, oriel_cs_130.wavelength );
}


/*--------------------------------------------------------*
 * Queries or sets the wavenumbera the current grating is set to
 *--------------------------------------------------------*/

Var_T *
monochromator_wavenumber( Var_T * v )
{
    double wn;
    double wl;
    int grating = oriel_cs_130.cur_grating;

    if ( ! v )
    {
        return vars_push( FLOAT_VAR, oriel_cs_130.wavelength != 0
                          ? oriel_cs_130_wl2wn( oriel_cs_130.wavelength )
                          : HUGE_VAL );
    }

    wn = get_double( v, "wavenumber" );
    too_many_arguments( v );

    if ( wn < 0 )
    {
        print( FATAL, "Invaid negative wavenumber.\n" );
        THROW( EXCEPTION );
    }

    if ( wn == 0 )
    {
        print( FATAL, "Invalid requested wavenumber of 0 cm^-1.\n" );
        THROW( EXCEPTION );
    }

    wl = oriel_cs_130_wn2wl( wn );

    if ( FSC2_MODE == EXPERIMENT )
    {
        double max_wl = 4.0 * oriel_cs_130.num_lines[ grating ] / 3;

        if ( wl > max_wl )
        {
            print( FATAL, "Requested Wavenumber of %.5f cm^-1 too small, "
                   "mimimum for grating %d is %.5f cm^-1.\n",
                   wn, grating + 1, oriel_cs_130_wl2wn( max_wl ) );
            THROW( EXCEPTION );
        }

        oriel_cs_130_set_wavelength( wl );
    }
    else
    {
        oriel_cs_130.wavelength = wl;
        oriel_cs_130.max_test_wavelength[ grating ] =
                       d_max( oriel_cs_130.max_test_wavelength[ grating ], wl );
    }

    return vars_push( FLOAT_VAR,
                      oriel_cs_130_wl2wn( oriel_cs_130.wavelength ) );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
monochromator_step_position( Var_T * v )
{
    long step;


    if ( ! v )
      return vars_push( INT_VAR, oriel_cs_130.step );

    step = get_long( v, "step position" );
    too_many_arguments( v );

    if ( step < 0 )
    {
        print( FATAL, "Invalid negative step position.\n" );
        THROW( EXCEPTION );
    }

    if ( step > MAX_STEP )
    {
        print( FATAL, "requested step position of %ld out of range, maximum "
               "is %d\n", step, MAX_STEP );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        oriel_cs_130_set_step( step );
    else
        oriel_cs_130.step = step;

    return vars_push( INT_VAR, step );
}


/*--------------------------------------------------------*
 * Queries or sets the groove density of a grating
 *--------------------------------------------------------*/

Var_T *
monochromator_groove_density( Var_T * v )
{
    long int grating;
    long int num_lines;


    if ( ! v )
        grating = oriel_cs_130.cur_grating;
    else
    {
        grating = get_strict_long( v, "grating number" ) - 1;
        v = vars_pop( v );
    }

    if ( grating < 0 && grating > NUM_GRATINGS )
    {
        print( FATAL, "Invalid grating number %ld, can only be between 1 and "
               "%d.\n", grating + 1, NUM_GRATINGS );
        THROW( EXCEPTION );
    }


    if ( ! v )
        return vars_push( FLOAT_VAR,
                          1000.0 * oriel_cs_130.num_lines[ grating ] );

    num_lines = lrnd( 0.001 * get_strict_long( v, "number of lines per m" ) );
    too_many_arguments( v );

    if ( num_lines <= 0 )
    {
        print( FATAL, "Invalid zero or negative number of lines per meter.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        oriel_cs_130_set_number_of_lines( grating, num_lines );
    else
        oriel_cs_130.num_lines[ grating ] = num_lines;

    return vars_push( FLOAT_VAR, 1000.0 * oriel_cs_130.num_lines[ grating ] );
}


/*--------------------------------------------------------*
 * Tell the monochromator about the current wavelength which it
 * then will use for calibration purposes.
 *--------------------------------------------------------*/

Var_T *
monochromator_calibrate( Var_T * v )
{
    double wl;


    if ( ! v )
    {
        print( FATAL, "Function requires a single argument, the current "
               "wavelength.\n" );
        THROW( EXCEPTION );
    }

    wl = get_double( v, "current wavelength" );

    if ( wl <= 0 )
    {
        print( FATAL, "Invalid zero or negative wavelength.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
    {
        double max_wl =
                4.0 * oriel_cs_130.num_lines[ oriel_cs_130.cur_grating ] / 3;

        if ( wl > max_wl )
        {
            print( FATAL, "Requested Wavelength of %.3f nm too large, maximum "
                   "for grating %d is %.3f nm.\n", wl * 1.0e9,
                   oriel_cs_130.cur_grating + 1, max_wl * 1.0e9 );
            THROW( EXCEPTION );
        }

        oriel_cs_130_calibrate( wl );
    }

    return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
monochromator_calibration_factor( Var_T * v )
{
    long int grating;
    double factor;


    if ( ! v )
        return vars_push( FLOAT_VAR,
                            oriel_cs_130.factor[ oriel_cs_130.cur_grating ] );

    grating = get_strict_long( v, "grating number" ) - 1;

    if ( grating < 1 || grating > NUM_GRATINGS )
    {
        print( FATAL, "Invalid grating number %ld, can only be between 1 and "
               "%d.\n", grating + 1, NUM_GRATINGS );
        THROW( EXCEPTION );
    }

    v = vars_pop( v );

    if ( ! v )
        return vars_push( FLOAT_VAR, oriel_cs_130.factor[ grating ] );

    factor = get_double( v, "calibration factor" );

    if ( factor <= 0 )
    {
        print( FATAL, "Invalid zero or negative calibration factor.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        oriel_cs_130_set_calibration_factor( grating, factor );
    else
        oriel_cs_130.factor[ grating ] = factor;

    return vars_push( FLOAT_VAR, oriel_cs_130.factor[ grating ] );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
monochromator_calibration_offset( Var_T * v )
{
    long int grating;
    double offset;


    if ( ! v )
        return vars_push( FLOAT_VAR,
                            oriel_cs_130.offset[ oriel_cs_130.cur_grating ] );

    grating = get_strict_long( v, "grating number" ) - 1;

    if ( grating < 1 || grating > NUM_GRATINGS )
    {
        print( FATAL, "Invalid grating number %ld, can only be between 1 and "
               "%d.\n", grating + 1, NUM_GRATINGS );
        THROW( EXCEPTION );
    }

    v = vars_pop( v );

    if ( ! v )
        return vars_push( FLOAT_VAR, oriel_cs_130.offset[ grating ] );

    offset = get_double( v, "calibration offset" );

    if ( FSC2_MODE == EXPERIMENT )
        oriel_cs_130_set_calibration_offset( grating, offset );
    else
        oriel_cs_130.offset[ grating ] = offset;

    return vars_push( FLOAT_VAR, oriel_cs_130.offset[ grating ] );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
monochromator_shutter( Var_T * v )
{
	bool state;

    if ( ! v )
		return vars_push( INT_VAR, ( long int ) oriel_cs_130.shutter_state );

	state = get_boolean( v );
	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
        oriel_cs_130_set_shutter( state );
    else if ( FSC2_MODE == TEST )
        oriel_cs_130.shutter_state = state;

	return vars_push( INT_VAR, ( long int ) oriel_cs_130.shutter_state );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

#if ! defined HAS_FILTER_WHEEL
Var_T *
monochromator_filter( Var_T * v  UNUSED_ARG )
{
    print( FATAL, "Module was not compiled with support for filters.\n" );
    THROW( EXCEPTION );
    return vars_push( INT_VAR, 0L );
}
#else
Var_T *
monochromator_filter( Var_T * v )
{
    long int filter;


#if 0
    if ( FSC2_MODE == TEST )
        oriel_cs_130.filter_has_been_used = SET;
#endif

    if ( v == 0 )
        return vars_push( INT_VAR, oriel_cs_130.filter + 1L );

    filter = get_strict_long( v, "filter number" ) - 1;
    too_many_arguments( v );

    if ( filter < 0 || filter >= MAX_FILTER )
    {
        print( FATAL, "Invalid filter number %ld, must be between 1 and %d.\n",
               filter, MAX_FILTER );
        THROW( EXCEPTION );
    }

    if ( filter != oriel_cs_130.filter )
    {
        if ( FSC2_MODE == EXPERIMENT )
            oriel_cs_130_set_filter( filter );
        else
            oriel_cs_130.filter = filter;
    }

    return vars_push( INT_VAR, oriel_cs_130.filter + 1L );
}
#endif


/*-------------------------------------------------------*
 * Resets the gratings zero values (back) to the values
 * given in the manual - only to be used in case the values
 * in the monochromators internal memory have become corrupted!
 *-------------------------------------------------------*/

Var_T *
monochromator_grating_zero( Var_T * v )
{
    long int grating;
    double zero;


    if ( ! v )
    {
        print( FATAL, "At least one argument, the grating number, is "
               "required.\n" );
        THROW( EXCEPTION );
    }

    grating = get_strict_long( v, "grating number" ) - 1;

    if ( grating < 1 || grating > NUM_GRATINGS )
    {
        print( FATAL, "Invalid grating number %ld, can only be between 1 and "
               "%d.\n", grating + 1, NUM_GRATINGS );
        THROW( EXCEPTION );
    }

    v = vars_pop( v );

    if ( ! v )
        return vars_push( FLOAT_VAR, oriel_cs_130.zero[ grating ] );

    zero = get_double( v, "grating zero" );

    if ( zero < 0 )
    {
        print( FATAL, "invalid negative grating zero value.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        oriel_cs_130_set_zero( grating, zero );
    else
        oriel_cs_130.zero[ grating ] = zero;

    return vars_push( FLOAT_VAR, oriel_cs_130.zero[ grating ] );
}


/*-------------------------------------------------------*
 * Allows to set a new GPIB address - can only be called during
 * PREPARATION section and, if the new address differes from the
 * one already set for the device, the EDL script stops after
 * having set the new address during initialization (since then
 * no further communication via the old address is possible
 * anymore).
 *-------------------------------------------------------*/

Var_T *
monochromator_set_gpib_address( Var_T * v )
{
    long int addr;


    if ( ! v )
    {
        print( FATAL, "Expected argument with new GPIB address.\n" );
        THROW( EXCEPTION );
    }

    addr = get_strict_long( v, "GPIB address" );
    too_many_arguments( v );

    if ( addr < 0 || addr > 30 )
    {
        print( FATAL, "Invalid GPIB address, mut be between 0 and 30.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == TEST )
        oriel_cs_130.gpib_address = addr;

    return vars_push( INT_VAR, addr );
}


/*-------------------------------------------------------*
 * Internal functions for initializing the monochromator
 *-------------------------------------------------------*/

static
bool
oriel_cs_130_init( const char * name )
{
    int i;

    if ( gpib_init_device( name, &oriel_cs_130.device ) == FAILURE )
        return FAIL;

    /* If we're asked to set a GPIB address differing from what the device
       reports do so and then stop because the device can now not be
       talked to anymore. */

    if (    oriel_cs_130.gpib_address >= 0
         && oriel_cs_130.gpib_address != oriel_cs_130_get_gpib_address( ) )
    {
        oriel_cs_130_set_gpib_address( oriel_cs_130.gpib_address );
        print( FATAL, "GPIB address has been changed, modify the GPIB "
               "configuration file and restart the EDL script.\n" );
        THROW( EXCEPTION );
    }

    /* Switch off "handshake" mode (the device sending a reply the status
       byte beside replie) */

    oriel_cs_130_command( "HANDSHAKE 0\n" );

    /* Clear a possibly set error byte by reading it */

    oriel_cs_130_get_error( );

    /* Switch to nanometer units */

    oriel_cs_130_command( "UNITS NM\n" );

    /* Determine which grating is in use and all information about both of
       them. Also check if during the test run a wavelength was requested
       that's larger than possible. */

    oriel_cs_130_get_grating( );

    for ( i = 0; i < NUM_GRATINGS; ++i )
    {
        oriel_cs_130_get_number_of_lines( i );
        oriel_cs_130_get_calibration_factor( i );
        oriel_cs_130_get_calibration_offset( i );
        oriel_cs_130_get_zero( i );

        double max_wl = 4.0 * oriel_cs_130.num_lines[ i ] / 3;

        if ( oriel_cs_130.max_test_wavelength[ i ] > max_wl )
        {
            print( FATAL, "During the test run a wavelength of %.3f nm was "
                   "requested for grating %d, which is larger than the "
                   "maximum of %.3f nm for that grating.\n",
                   oriel_cs_130.max_test_wavelength[ i ], i + 1, max_wl );
            THROW( EXCEPTION );
        }
    }

    /* Get the current state of the shutter and the filter wheel (if it
       exists) */

    oriel_cs_130_get_shutter( );

#if defined HAS_FILTER_WHEEL
    oriel_cs_130_get_filter( );
#endif

    return OK;
}


/*----------------------------------------------*
 * Queries the currenty used grating, returns 0
 * for the first and 1 for the second
 *----------------------------------------------*/

static
int
oriel_cs_130_get_grating( void )
{
    char reply[ 100 ];


    if (    oriel_cs_130_talk( "GRAT?\n", reply, sizeof reply ) < 1
         || ( reply[ 0 ] - '0' < 1 || reply[ 0 ] - '0' > NUM_GRATINGS ) )
        oriel_cs_130_failure( );

    return oriel_cs_130.cur_grating = reply[ 0 ] - '1';
}


/*----------------------------------------------*
 * Sets a new grating after closing the shutter,
 * argument is 0 for grating #1 and 1 for and #2
 *----------------------------------------------*/

static
int
oriel_cs_130_set_grating( int grating )
{
    char cmd[ ] = "GRAT *\n";


    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );

    if ( oriel_cs_130.cur_grating == grating )
        return grating;

    if ( ! oriel_cs_130.shutter_state )
        oriel_cs_130_set_shutter( SET );

    cmd[ 5 ] = '1' + grating;
    oriel_cs_130_command( cmd );

    oriel_cs_130.cur_grating = oriel_cs_130_get_grating( );
    oriel_cs_130_get_wavelength( );

    return oriel_cs_130.cur_grating;
}


/*--------------------------------*
 * Returns the number of lines of a grating (in lines/mm).
 * The argument is the grating number, 0 for garting #1
 * and 1 for grating #2.
 *--------------------------------*/

static
unsigned int
oriel_cs_130_get_number_of_lines( int grating )
{
    char req[ ] = "GRAT*LINES?\n";
    char reply[ 100 ];
    unsigned long val;
    char * ep;


    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );

    req[ 4 ] = grating + '1';
    if (    oriel_cs_130_talk( req, reply, sizeof reply ) < 1
         || ! isdigit( ( int ) reply[ 0 ] ) )
        oriel_cs_130_failure( );

    val = strtoul( reply, &ep, 10 );
    if ( *ep || val == 0 || val == ULONG_MAX )
        oriel_cs_130_failure( );

    return oriel_cs_130.num_lines[ grating ] = val;
}


/*--------------------------------*
 * Set the number of lines of a grating. The first argument
 * is the grating number, 0 for garting #1 and 1 for grating
 * #2, the second the number of lines (per mm)
 *--------------------------------*/

static
unsigned int
oriel_cs_130_set_number_of_lines( int          grating,
                                  unsigned int num_lines )
{
    char cmd[ 100 ];

    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );
    fsc2_assert( num_lines > 0 );

    if ( oriel_cs_130.num_lines[ grating ] == num_lines )
        return num_lines;

    sprintf( cmd, "GRAT%dLINES %u\n", grating + 1, num_lines );
    oriel_cs_130_command( cmd );

    return oriel_cs_130.num_lines[ grating ] = num_lines;
}


/*--------------------------------*
 *--------------------------------*/

static
void
oriel_cs_130_calibrate( double wl )
{
    char cmd[ 100 ];

    sprintf( cmd, "CALIBRATE %.3f\n", wl );
    oriel_cs_130_command( cmd );
}


/*--------------------------------*
 * Returns the calibration factor for the given grating.
 * The argument is the grating number, 0 for garting #1
 * and 1 for grating #2.
 *--------------------------------*/

static double
oriel_cs_130_get_calibration_factor( int grating )
{
    char req[ ] = "GRAT*FACTOR?\n";
    char reply[ 100 ];
    double val;
    char * ep;


    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );

    req[ 4 ] = grating + '1';
    if (    oriel_cs_130_talk( req, reply, sizeof reply ) < 1
         || ! isdigit( ( int ) reply[ 0 ] ) )
        oriel_cs_130_failure( );

    val = strtod( reply, &ep );
    if ( *ep || val == 0 || val == HUGE_VAL )
        oriel_cs_130_failure( );

    return oriel_cs_130.factor[ grating ] = val;
}


/*--------------------------------*
 * Set a new calibration factor for the given grating.
 * First argument is the grating number, 0 for garting
 * #1 and 1 for #2, second argument the new factor.
 *--------------------------------*/

static
double
oriel_cs_130_set_calibration_factor( int    grating,
                                     double factor )
{
    char cmd[ 100 ];

    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );
    fsc2_assert( factor >= 1.0e-6 );

    if ( factor == oriel_cs_130.factor[ grating ] )
        return factor;

    sprintf( cmd, "GRAT%dFACTOR %.6f\n", grating + 1, factor );
    oriel_cs_130_command( cmd );

    return oriel_cs_130.factor[ grating ] = factor;
}


/*--------------------------------*
 * Returns the calibration offset for the given grating.
 * The argument is the grating number, 0 for garting #1
 * and 1 for grating #2.
 *--------------------------------*/

static
double
oriel_cs_130_get_calibration_offset( int grating )
{
    char req[ ] = "GRAT*OFFSTE?\n";
    char reply[ 100 ];
    double val;
    char * ep;


    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );

    req[ 4 ] = grating + '1';
    if (    oriel_cs_130_talk( req, reply, sizeof reply ) < 1
         || (    reply[ 0 ] != '-'
              && reply[ 0 ] != '+'
              && ! isdigit( ( int ) reply[ 0 ] ) ) )
        oriel_cs_130_failure( );

    val = strtod( reply, &ep );
    if ( *ep || val == - HUGE_VAL || val == HUGE_VAL )
        oriel_cs_130_failure( );

    return oriel_cs_130.offset[ grating ] = val;
}


/*--------------------------------*
 * Set a new calibration offset for the given grating.
 * First argument is the grating number, 0 for garting #1
 * and 1 for #2, second argument the new offset.
 *--------------------------------*/

static
double
oriel_cs_130_set_calibration_offset( int    grating,
                                     double offset )
{
    char cmd[ 100 ];


    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );

    if ( offset == oriel_cs_130.offset[ grating ] )
        return offset;

    sprintf( cmd, "GRAT%dOFFSET %.6f\n", grating + 1, offset );
    oriel_cs_130_command( cmd );

    return oriel_cs_130.offset[ grating ] = offset;
}


/*--------------------------------*
 * Sets a gratings zero value.
 *--------------------------------*/

static
double
oriel_cs_130_get_zero( int    grating )
{
    char cmd[ ] = "GRAT*ZERO?\n";
    char reply[ 100 ];
    double zero;
    char *ep;


    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );

    cmd[ 4 ] = grating + '1';
    
    if ( oriel_cs_130_talk( cmd, reply, sizeof reply ) < 1 )
        oriel_cs_130_failure( );

    zero = strtod( reply, &ep );
    if ( *ep || zero < 0 || zero == HUGE_VAL )
        oriel_cs_130_failure( );

    return oriel_cs_130.zero[ grating ] = zero;
}


/*--------------------------------*
 * Sets a gratings zero value.
 *--------------------------------*/

static
void
oriel_cs_130_set_zero( int    grating,
                       double zero )
{
    char buf[ 100 ];


    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );
    fsc2_assert( zero >= 0 );

    sprintf( buf, "GRAT%dZERO %.7f\n", grating + 1, zero );
    oriel_cs_130_command( buf );

    oriel_cs_130.zero[ grating ] = zero;
}


/*--------------------------------*
 * Returns the eaveength the monocromator is currently set to.
 *--------------------------------*/

static double
oriel_cs_130_get_wavelength( void )
{
    char reply[ 100 ];
    double val;
    char *ep;


    if (    oriel_cs_130_talk( "WAVE?\n", reply, sizeof reply ) < 1
         || ! isdigit( ( int ) reply[ 0 ] ) )
        oriel_cs_130_failure( );

    val = strtod( reply, &ep );
    if ( *ep || val == -HUGE_VAL || val == HUGE_VAL )
        oriel_cs_130_failure( );

    return oriel_cs_130.wavelength = 1.0e-9 * val;
}


/*--------------------------------*
 * Moves the monochromator to the given wavelength (or the
 * nearest possible one). Might thwo an exception if the
 * requested wavelength can't be set. Returns the exact
 * new wavelength as reported by the device.
 *--------------------------------*/

static
double
oriel_cs_130_set_wavelength( double wl )
{
    char cmd[ 100 ];


    fsc2_assert( wl >= 0 );

    sprintf( cmd, "GOWAVE %.3f\n", 1.0e9 * wl );

    /* Make sure we don't set a wavelnegth that's already set */

    wl = 1.0e-9 * strtod( cmd + 7, NULL );
    if ( wl == oriel_cs_130.wavelength )
        return wl;

    sprintf( cmd, "GOWAVE %.3f\n", 1.0e9 * wl );
    oriel_cs_130_command( cmd );

    /* Check for errors - the value might be out of range */

    switch ( oriel_cs_130_get_error( ) )
    {
        case -1 :
            break;

        case 3 :
            print( FATAL, "Device reports requested wavelength of %.3f nm to "
                   "be out of range.\n", wl );
            THROW( EXCEPTION );

        default :
            oriel_cs_130_failure( );
    }

    /* Get the exact wavelength and step position from the device */

    oriel_cs_130_get_step( );
    return oriel_cs_130_get_wavelength( );
}


/*--------------------------------*
 * Returns the current "step position"
 *--------------------------------*/

static
unsigned int
oriel_cs_130_get_step( void )
{
    char reply[ 100 ];
    unsigned long step;
    char *ep;


    if (    oriel_cs_130_talk( "STEP?\n", reply, sizeof reply ) < 1
            || ! isdigit( ( int ) reply[ 0 ] ) )
        oriel_cs_130_failure( );

    step = strtoul( reply, &ep, 10 );
    if ( *ep || step > UINT_MAX )
        oriel_cs_130_failure( );

    return oriel_cs_130.step = step;
}


/*--------------------------------*
 * Moves the monochromator by the given number of steps
 *--------------------------------*/

static
unsigned int
oriel_cs_130_set_step( unsigned int step )
{
    char cmd[ 100 ];


    fsc2_assert( step <= MAX_STEP );

    if ( step == oriel_cs_130.step )
        return step;

    sprintf( cmd, "STEP %u\n", step );
    fprintf( stderr, "%s", cmd );
    oriel_cs_130_command( cmd );

    switch ( oriel_cs_130_get_error( ) )
    {
        case -1 :
            break;

        case 3 :
            print( FATAL, "Device reports requested step %u to be out of "
                   "range.\n", step );
            THROW( EXCEPTION );

        default :
            oriel_cs_130_failure( );
    }

    oriel_cs_130_get_wavelength( );
    return oriel_cs_130_get_step( );
}


/*--------------------------------*
 * Returns if the shutter is closed
 *--------------------------------*/

static
bool
oriel_cs_130_get_shutter( void )
{
    char reply[ 100 ];


    if (    oriel_cs_130_talk( "SHUTTER?\n", reply, sizeof reply ) != 1
         || ( reply[ 0 ] != 'C' && reply[ 0 ] != 'O' ) )
        oriel_cs_130_failure( );

    return oriel_cs_130.shutter_state = reply[ 0 ] == 'C';
}


/*--------------------------------*
 * Opens (if called with a 'false' value) or closes
 *  (if called with a 'true' value) the shutter
 *--------------------------------*/

static
bool
oriel_cs_130_set_shutter( bool on_off )
{
    char cmd[ ] = "SHUTTER *\n";


    if ( on_off == oriel_cs_130.shutter_state )
        return on_off;

    cmd[ 8 ] = on_off ? 'C' : 'O';
    oriel_cs_130_command( cmd );

	return oriel_cs_130.shutter_state = on_off;
}


/*----------------------------------------------------*
 * Returns the number of the current filter, a number
 * between 0 and 5, or -1 if the filter wheel is out
 * of position.
 *----------------------------------------------------*/

#if defined HAS_FILTER_WHEEL
static
int
oriel_cs_130_get_filter( void )
{
    char reply[ 100 ];


    if (    oriel_cs_130_talk( "FILTER?\n", reply,sizeof reply ) != 1
         || ! isdigit( ( int ) reply[ 0 ] ) )
        oriel_cs_130_failure( );

    /* If '0' got returned the filter wheel is out of position (or there
       isn't one) and we need to reset the error byte by reading it */

    if ( reply[ 0 ] == '0' )
    {
        if ( oriel_cs_130_get_error( ) == 6 )
            print( FATAL, "Device has no filter wheel.\n" );
        else
            print( FATAL, "Filter wheel is out of position.\n" );
        THROW( EXCEPTION );
    }
    else if ( reply[ 0 ] - '0' > MAX_FILTER )
        oriel_cs_130_failure( );

    return oriel_cs_130.filter = reply[ 0 ] - '1';
}
#endif


/*----------------------------------------------------*
 * Switches to a new filter and waits for it to be in position.
 *----------------------------------------------------*/

#if defined HAS_FILTER_WHEEL
static
int
oriel_cs_130_set_filter( int filter )
{
    char cmd[ ] = "FILTER *\n";

    fsc2_assert( filter >= 0 && filter < MAX_FILTER );

    if ( filter == oriel_cs_130.filter )
        return filter;

    cmd[ 7 ] = '1' + filter;
    oriel_cs_130_command( cmd );

    /* Wait for the new filter position to become set */

    do
      fsc2_usleep( 100000, UNSET );
    while ( oriel_cs_130_get_filter( ) != filter );

    return oriel_cs_130.filter = filter;
}
#endif


/*--------------------------------------------------*
 * Reads the status byte and if this indicates that there was
 * an error, the error error code. Returns -1 if there was no
 * error, otherwise the error code.
 *--------------------------------------------------*/

static
int
oriel_cs_130_get_error( void )
{
    unsigned long int b;
    char reply[ 4 ];
    char *ep;


    if (    oriel_cs_130_talk( "STB?\n", reply, sizeof reply ) < 1
         || ! isdigit( ( int ) reply[ 0 ] ) )
        oriel_cs_130_failure( );

    b = strtoul( reply, &ep, 10 );
    if ( *ep || b == ULONG_MAX )
        oriel_cs_130_failure( );

    if ( b == 0 )
        return -1;

    if (    oriel_cs_130_talk( "ERROR?\n", reply, sizeof reply ) != 1
         || ! isdigit( ( int ) reply[ 0 ] ) )
        oriel_cs_130_failure( );

    return reply[ 0 ] - '0';
}

/*--------------------------------*
 * Sends a message to the device.
 *--------------------------------*/

static
void
oriel_cs_130_command( const char * cmd )
{
    if ( gpib_write( oriel_cs_130.device, cmd, strlen( cmd ) ) == FAILURE )
        oriel_cs_130_failure( );
}


/*-------------------------------------*
 * Sends a message and returns the reply (after testing for and removing
 * the trailing '\r' and '\n') and its length
 *-------------------------------------*/

static
long
oriel_cs_130_talk( const char * cmd,
                   char       * reply,
                   long         length )
{
    if (    gpib_write( oriel_cs_130.device, cmd, strlen( cmd ) ) == FAILURE
         || gpib_read( oriel_cs_130.device, reply, &length ) == FAILURE
         || reply[ length - 2 ] != '\r'
         || reply[ length - 1 ] != '\n' )
        oriel_cs_130_failure( );

    reply[ length -= 2 ] = '\0';

    return length;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static void
oriel_cs_130_failure( void )
{
    print( FATAL, "Communication with monochromator failed.\n" );
    THROW( EXCEPTION );
}


/*-----------------------------------------*
 * Returns the currently used GPIB address
 *-----------------------------------------*/

static
int
oriel_cs_130_get_gpib_address( void )
{
    char reply[ 100 ];
    unsigned long addr;
    char *ep;


    if (    oriel_cs_130_talk( "ADDRESS?\n", reply, sizeof reply ) < 1
            || ! isdigit( ( int ) reply[ 0 ] ) )
        oriel_cs_130_failure( );

    addr = strtoul( reply, &ep, 10 );
    if ( *ep || addr > 30 )
        oriel_cs_130_failure( );

    return addr;
}


/*-----------------------------------------*
 * Sets a new GPIB address. That's the only way to change it and
 * when this has been done we can't continue talking to the device...
 *-----------------------------------------*/

static
void
oriel_cs_130_set_gpib_address( unsigned int addr )
{
    char cmd[ 100 ];


    fsc2_assert( addr <= 30 );

    if ( ( int ) addr == oriel_cs_130_get_gpib_address( ) )
        return;

    sprintf( cmd, "ADDRESS %2u\n", addr );
    oriel_cs_130_command( cmd );
}


/*-----------------------------------------*
 * Converts a wavelength into a wavenumber
 *-----------------------------------------*/

static
double
oriel_cs_130_wl2wn( double wl )
{
    fsc2_assert( wl > 0 );
    return 0.01 / wl;
}


/*-----------------------------------------*
 * Converts a wavenumber into a wavelength
 *-----------------------------------------*/

static
double
oriel_cs_130_wn2wl( double wn )
{
    fsc2_assert( wn > 0 );
    return 0.01 / wn;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
