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
#include <sys/time.h>


/* Include configuration information for the device */

#include "oriel_cs_260.conf"

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
    int          grating;                        /* 0: grating #1, etc. */

    double       wavelength;                     /* in m */

    long int position;

    unsigned int num_lines[ NUM_GRATINGS ];      /* in lines per mm */
    double       factor[ NUM_GRATINGS ];
    double       offset[ NUM_GRATINGS ];
    double       zero[ NUM_GRATINGS ];

    bool         shutter_state;                  /* true if shutter closed */

    int          filter;                         /* 0: filter #1, etc. */

    int outport;

    int gpib_address;

    struct timeval last_shutter_open,            /* last time shutter was */
                   last_shutter_close;           /* opened or closed */
    
} Oriel_CS_260;


static Oriel_CS_260 oriel_cs_260;


/* Limits for wavelengths and stepper motor positions and shutter exposure
   and repetition time (in seconds) */

#if NUM_GRATINGS != 2 && NUM_GRATINGS != 3
#error "Configuration error, NUM_GRATINGS must be 2 or 3."
#endif

#if NUM_GRATINGS == 2
static double Max_Wavelengths[ 2 ] = { 1.0e-9 * GRATING_1_MAX_WAVELENGTH,
                                       1.0e-9 * GRATING_2_MAX_WAVELENGTH };

static long Position_Ranges[ 2 ][ 2 ] =
                        { { GRATING_1_MIN_POSITION, GRATING_1_MAX_POSITION },
                          { GRATING_2_MIN_POSITION, GRATING_2_MAX_POSITION } };
#endif

#if NUM_GRATINGS == 3
static double Max_Wavelengths[ 3 ] = { 1.0e-9 * GRATING_1_MAX_WAVELENGTH,
                                       1.0e-9 * GRATING_2_MAX_WAVELENGTH,
                                       1.0e-9 * GRATING_3_MAX_WAVELENGTH };

static long Position_Ranges[ 3 ][ 2 ] =
                        { { GRATING_1_MIN_POSITION, GRATING_1_MAX_POSITION },
                          { GRATING_2_MIN_POSITION, GRATING_2_MAX_POSITION },
                          { GRATING_3_MIN_POSITION, GRATING_3_MAX_POSITION } };
#endif

#define MIN_SHUTTER_EXPOSURE_TIME  0.2
#define MIN_SHUTTER_REPEAT_TIME    2.0       /* 0.5 Hz */


/* Times we are prepared to wait for an action to complete (in seconds) */

static double Max_Grating_Switch_Delay = 30;
static double Max_Wavelength_Delay     = 20;
static double Max_Shutter_Delay        = 0.2;
static double Max_Filter_Delay         = 10;
static double Max_Outport_Switch_Delay = 10;


/* Values for test run */

#define TEST_WAVELENGTH      5.5e-7
#define TEST_POSITION        27000
#define TEST_GRATING_NUMBER  0
#define TEST_SHUTTER_STATE   0
#define TEST_FILTER          0
#define TEST_OUTPORT         0

/* Hook functions */

int oriel_cs_260_init_hook( void );
int oriel_cs_260_exp_hook( void );
 

/* EDLfunctions */

Var_T * monochromator_name( Var_T * v  UNUSED_ARG );
Var_T * monochromator_grating( Var_T * v );
Var_T * monochromator_wavelength( Var_T * v );
Var_T * monochromator_wavenumber( Var_T * v );
Var_T * monochromator_step( Var_T * v );
Var_T * monochromator_groove_density( Var_T * v );
Var_T * monochromator_calibrate( Var_T *v );
Var_T * monochromator_calibration_factor( Var_T * v );
Var_T * monochromator_calibration_offset( Var_T * v );
Var_T * monochromator_shutter( Var_T * v );
Var_T * monochromator_filter( Var_T * v );
Var_T * monochromator_output_port( Var_T * v );
Var_T * monochromator_grating_zero( Var_T * v );
Var_T * monochromator_set_gpib_address( Var_T * v );


/* Internally used functions */

static bool oriel_cs_260_init( const char * name );
static int oriel_cs_260_get_grating( void );
static int oriel_cs_260_set_grating( int grating );
static unsigned int oriel_cs_260_get_number_of_lines( int grating );
static unsigned int oriel_cs_260_set_number_of_lines( int          grating,
                                                      unsigned int num_lines );
static void oriel_cs_260_calibrate( double wl );
static double oriel_cs_260_get_calibration_factor( int grating );
static double oriel_cs_260_set_calibration_factor( int    grating,
                                                   double factor );
static double oriel_cs_260_get_calibration_offset( int grating );
static double oriel_cs_260_set_calibration_offset( int    grating,
                                                   double offset );
static double oriel_cs_260_get_zero( int    gratings );
static void oriel_cs_260_set_zero( int    gratings,
                                   double zero );
static double oriel_cs_260_get_wavelength( void );
static double oriel_cs_260_set_wavelength( double wl );
static long int oriel_cs_260_get_position( void );
static bool oriel_cs_260_do_step( long int step );
static bool oriel_cs_260_get_shutter( void );
static bool oriel_cs_260_set_shutter( bool on_off );
#if defined HAS_FILTER_WHEEL
static int oriel_cs_260_get_filter( void );
static int oriel_cs_260_set_filter( int filter );
#endif
static int oriel_cs_260_get_outport( void );
static int oriel_cs_260_set_outport( int port );
static int oriel_cs_260_get_error( void );
static bool oriel_cs_260_command( const char * cmd,
                                  double       wait_for );
static long oriel_cs_260_talk( const char * cmd,
                               char       * reply,
                               long         length,
                               bool         exact_length );
static void oriel_cs_260_failure( void );
static int oriel_cs_260_get_gpib_address( void );
static void oriel_cs_260_set_gpib_address( unsigned int addr );
static double oriel_cs_260_wl2wn( double wl );
static double oriel_cs_260_wn2wl( double wn );


/*------------------------------------*
 * Init hook function for the module.
 *------------------------------------*/

int
oriel_cs_260_init_hook( void )
{
    int i;


    /* Set global variable to indicate that GPIB bus is needed */

    Need_GPIB = SET;

    /* Reset several variables in the structure describing the device */

    oriel_cs_260.device        = -1;

    oriel_cs_260.wavelength = TEST_WAVELENGTH;
    oriel_cs_260.position = TEST_POSITION;

    oriel_cs_260.grating = TEST_GRATING_NUMBER;

    for ( i = 0; i < NUM_GRATINGS; i++ )
    {
        oriel_cs_260.num_lines[ i ] = 600;
        oriel_cs_260.factor[ i ] = 1;
        oriel_cs_260.offset[ i ] = 0.025;
        oriel_cs_260.zero[ i ] = i * 2 * M_PI / NUM_GRATINGS + 0.0872665;
    }

    oriel_cs_260.shutter_state = TEST_SHUTTER_STATE;

    oriel_cs_260.filter = TEST_FILTER;

    oriel_cs_260.outport = TEST_OUTPORT;

    oriel_cs_260.gpib_address = -1;

    return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int
oriel_cs_260_exp_hook( void )
{
    /* Initialize the device */

    if ( oriel_cs_260_init( DEVICE_NAME ) )
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
            oriel_cs_260_set_grating( grating - 1 );
        else
            oriel_cs_260.grating = grating - 1;
    }

    return vars_push( INT_VAR, oriel_cs_260.grating + 1L );
}


/*--------------------------------------------------------*
 * Queries or sets the wavelength the current grating is set to
 *--------------------------------------------------------*/

Var_T *
monochromator_wavelength( Var_T * v )
{
    int grating = oriel_cs_260.grating;
    double wl;


    if ( ! v )
        return vars_push( FLOAT_VAR, oriel_cs_260.wavelength );

    wl = get_double( v, "wavelength" );
    too_many_arguments( v );

    if ( wl == oriel_cs_260.wavelength )
        return vars_push( FLOAT_VAR, oriel_cs_260.wavelength );

    if ( wl <= 0 )
    {
        print( FATAL, "Invaid zero or negative wavelength.\n" );
        THROW( EXCEPTION );
    }

    if ( wl > Max_Wavelengths[ grating ] )
    {
        print( FATAL, "Requested Wavelength of %.3f nm too large, maximum "
               "for grating #%d is %.3f nm.\n", wl * 1.0e9, grating + 1,
               1.0e9 * Max_Wavelengths[ grating ] );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        oriel_cs_260_set_wavelength( wl );

    return vars_push( FLOAT_VAR, oriel_cs_260.wavelength );
}


/*--------------------------------------------------------*
 * Queries or sets the wavenumbera the current grating is set to
 *--------------------------------------------------------*/

Var_T *
monochromator_wavenumber( Var_T * v )
{
    double wn;
    double wl;
    int grating = oriel_cs_260.grating;

    if ( ! v )
    {
        return vars_push( FLOAT_VAR, oriel_cs_260.wavelength != 0
                          ? oriel_cs_260_wl2wn( oriel_cs_260.wavelength )
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

    wl = oriel_cs_260_wn2wl( wn );

    if ( wl > Max_Wavelengths[ grating ] )
    {
        print( FATAL, "Requested Wavenumber of %.5f cm^-1 too small, "
               "mimimum for grating #%d is %.5f cm^-1.\n",
               wn, grating + 1,
               oriel_cs_260_wl2wn( Max_Wavelengths[ grating ] ) );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        oriel_cs_260_set_wavelength( wl );

    return vars_push( FLOAT_VAR,
                      oriel_cs_260_wl2wn( oriel_cs_260.wavelength ) );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
monochromator_step( Var_T * v )
{
    long step;
    int grat = oriel_cs_260.grating;
    bool test_only = UNSET;

    /* Witout no arguments return the cirrent stepper motor position */

    if ( ! v )
      return vars_push( INT_VAR, oriel_cs_260.position );

    /* Get the step size and, if given, a flag that can be used to make
       this just a test if the step would be posible */

    step = get_long( v, "step size" );

    if ( ( v = vars_pop( v ) ) )
        test_only = get_boolean( v );

    too_many_arguments( v );

    /* In test mode we always succeed since there's no way we could forsee
       if this will work in real */

    if ( FSC2_MODE == TEST )
    {
        if ( ! test_only )
            oriel_cs_260.position += step;
        return vars_push( INT_VAR, 1L );
    }

    /* Check if making that step is possible - if not return false
       (and, if the test flag isn't set, also emit a warning) */

    if (    oriel_cs_260.position + step < Position_Ranges[ grat ][ 0 ]
         || oriel_cs_260.position + step > Position_Ranges[ grat ][ 1 ] )
    {
        if ( ! test_only )
            print( WARN, "Can't change position by %ld, out of range.\n",
                   step );
        return vars_push( INT_VAR, 0L );
    }

    /* Now try to do the step - it still might fail */

    if ( ! test_only && ! oriel_cs_260_do_step( step ) )
        return vars_push( INT_VAR, 0L );

    return vars_push( INT_VAR, 1L );
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
        grating = oriel_cs_260.grating;
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
                          1000.0 * oriel_cs_260.num_lines[ grating ] );

    num_lines = lrnd( 0.001 * get_strict_long( v, "number of lines per m" ) );
    too_many_arguments( v );

    if ( num_lines <= 0 )
    {
        print( FATAL, "Invalid zero or negative number of lines per meter.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        oriel_cs_260_set_number_of_lines( grating, num_lines );
    else
        oriel_cs_260.num_lines[ grating ] = num_lines;

    return vars_push( FLOAT_VAR, 1000.0 * oriel_cs_260.num_lines[ grating ] );
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
                4.0 * oriel_cs_260.num_lines[ oriel_cs_260.grating ] / 3;

        if ( wl > max_wl )
        {
            print( FATAL, "Requested Wavelength of %.3f nm too large, maximum "
                   "for grating #%d is %.3f nm.\n", wl * 1.0e9,
                   oriel_cs_260.grating + 1, max_wl * 1.0e9 );
            THROW( EXCEPTION );
        }

        oriel_cs_260_calibrate( wl );
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
                            oriel_cs_260.factor[ oriel_cs_260.grating ] );

    grating = get_strict_long( v, "grating number" ) - 1;

    if ( grating < 1 || grating > NUM_GRATINGS )
    {
        print( FATAL, "Invalid grating number %ld, can only be between 1 and "
               "%d.\n", grating + 1, NUM_GRATINGS );
        THROW( EXCEPTION );
    }

    v = vars_pop( v );

    if ( ! v )
        return vars_push( FLOAT_VAR, oriel_cs_260.factor[ grating ] );

    factor = get_double( v, "calibration factor" );

    if ( factor <= 0 )
    {
        print( FATAL, "Invalid zero or negative calibration factor.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        oriel_cs_260_set_calibration_factor( grating, factor );
    else
        oriel_cs_260.factor[ grating ] = factor;

    return vars_push( FLOAT_VAR, oriel_cs_260.factor[ grating ] );
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
                            oriel_cs_260.offset[ oriel_cs_260.grating ] );

    grating = get_strict_long( v, "grating number" ) - 1;

    if ( grating < 1 || grating > NUM_GRATINGS )
    {
        print( FATAL, "Invalid grating number %ld, can only be between 1 and "
               "%d.\n", grating + 1, NUM_GRATINGS );
        THROW( EXCEPTION );
    }

    v = vars_pop( v );

    if ( ! v )
        return vars_push( FLOAT_VAR, oriel_cs_260.offset[ grating ] );

    offset = get_double( v, "calibration offset" );

    if ( FSC2_MODE == EXPERIMENT )
        oriel_cs_260_set_calibration_offset( grating, offset );
    else
        oriel_cs_260.offset[ grating ] = offset;

    return vars_push( FLOAT_VAR, oriel_cs_260.offset[ grating ] );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
monochromator_shutter( Var_T * v )
{
	bool state;

    if ( ! v )
		return vars_push( INT_VAR, ( long int ) oriel_cs_260.shutter_state );

	state = get_boolean( v );
	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
        oriel_cs_260_set_shutter( state );
    else if ( FSC2_MODE == TEST )
        oriel_cs_260.shutter_state = state;

	return vars_push( INT_VAR, ( long int ) oriel_cs_260.shutter_state );
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


    if ( v == 0 )
        return vars_push( INT_VAR, oriel_cs_260.filter + 1L );

    filter = get_strict_long( v, "filter number" ) - 1;
    too_many_arguments( v );

    if ( filter < 0 || filter >= NUM_FILTERS )
    {
        print( FATAL, "Invalid filter number %ld, must be between 1 and %d.\n",
               filter, NUM_FILTERS );
        THROW( EXCEPTION );
    }

    if ( filter != oriel_cs_260.filter )
    {
        if ( FSC2_MODE == EXPERIMENT )
            oriel_cs_260_set_filter( filter );
        else
            oriel_cs_260.filter = filter;
    }

    return vars_push( INT_VAR, oriel_cs_260.filter + 1L );
}
#endif


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
monochromator_output_port( Var_T * v )
{
    int outport;


    if ( v == 0 )
        return vars_push( INT_VAR, oriel_cs_260.outport + 1L );

    if ( v->type != STR_VAR )
    {
        print( FATAL, "Invalid argument, must be either the string \"AXIAL\" "
               "or \"LATERAL\".\n" );
        THROW( EXCEPTION );
    }

    if ( ! strcasecmp( v->val.sptr, "AXIAL" ) )
        outport = 0;
    else if ( ! strcasecmp( v->val.sptr, "LATERAL" ) )
        outport = 1;
    else
    {
        print( FATAL, "Invalid argument, must be either the \"AXIAL\" or "
               "\"LATERAL\".\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        oriel_cs_260_set_outport( outport );
    else
        oriel_cs_260.outport = outport;

    return vars_push( STR_VAR, oriel_cs_260.outport + 1L );
}


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
        return vars_push( FLOAT_VAR, oriel_cs_260.zero[ grating ] );

    zero = get_double( v, "grating zero" );

    if ( zero < 0 )
    {
        print( FATAL, "invalid negative grating zero value.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        oriel_cs_260_set_zero( grating, zero );
    else
        oriel_cs_260.zero[ grating ] = zero;

    return vars_push( FLOAT_VAR, oriel_cs_260.zero[ grating ] );
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
        oriel_cs_260.gpib_address = addr;

    return vars_push( INT_VAR, addr );
}


/*-------------------------------------------------------*
 * Internal functions for initializing the monochromator
 *-------------------------------------------------------*/

static
bool
oriel_cs_260_init( const char * name )
{
    int i;

    if ( gpib_init_device( name, &oriel_cs_260.device ) == FAILURE )
        return FAIL;

    /* If we're asked to set a GPIB address differing from what the device
       reports do so and then stop because the device can now not be
       talked to anymore. */

    if (    oriel_cs_260.gpib_address >= 0
         && oriel_cs_260.gpib_address != oriel_cs_260_get_gpib_address( ) )
    {
        oriel_cs_260_set_gpib_address( oriel_cs_260.gpib_address );
        print( FATAL, "GPIB address has been changed, now modify the GPIB "
               "configuration file and restart the EDL script.\n" );
        THROW( EXCEPTION );
    }

    /* Switch on "handshake" mode (the device sends the status byte beside
       replies - this helps to identify when certain commands that take a
       lot of time are finished) */

    if ( ! oriel_cs_260_command( "HANDSHAKE 1\n", 0 ) )
        return UNSET;

    /* Clear a possibly set error byte by reading it */

    fprintf( stderr, "Checking error value\n" );
    oriel_cs_260_get_error( );

    /* Switch to nanometer units */

    if ( ! oriel_cs_260_command( "UNITS NM\n", 0 ) )
        return UNSET;

    /* Determine which grating is in use and all information about both of
       them. Also check if during the test run a wavelength was requested
       that's larger than possible. */

    oriel_cs_260_get_grating( );
    oriel_cs_260_get_wavelength( );
    oriel_cs_260_get_position( );

    for ( i = 0; i < NUM_GRATINGS; ++i )
    {
        oriel_cs_260_get_number_of_lines( i );
        oriel_cs_260_get_calibration_factor( i );
        oriel_cs_260_get_calibration_offset( i );
        oriel_cs_260_get_zero( i );
    }

    oriel_cs_260_get_outport( );

    /* Get the current state of the shutter and set times for last closed
       and opened */

    oriel_cs_260_get_shutter( );
    gettimeofday( &oriel_cs_260.last_shutter_open, NULL );
    gettimeofday( &oriel_cs_260.last_shutter_close, NULL );

    /* If available get current filter */

#if defined HAS_FILTER_WHEEL
    oriel_cs_260_get_filter( );
#endif

    return OK;
}


/*----------------------------------------------*
 * Queries the currenty used grating, returns 0
 * for the first and 1 for the second
 *----------------------------------------------*/

static
int
oriel_cs_260_get_grating( void )
{
    char reply[ 3 ];


    if (    oriel_cs_260_talk( "GRAT?\n", reply, sizeof reply, SET ) != 1
         || ! isdigit( ( int ) *reply )
         || ( *reply - '0' < 1 || *reply - '0' > NUM_GRATINGS ) )
        oriel_cs_260_failure( );

    return oriel_cs_260.grating = *reply - '1';
}


/*----------------------------------------------*
 * Sets a new grating after closing the shutter,
 * argument is 0 for grating #1 and 1 for and #2
 *----------------------------------------------*/

static
int
oriel_cs_260_set_grating( int grating )
{
    char cmd[ ] = "GRAT *\n";


    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );

    if ( oriel_cs_260.grating == grating )
        return grating;

    if ( ! oriel_cs_260.shutter_state )
        oriel_cs_260_set_shutter( SET );

    cmd[ 5 ] = '1' + grating;
    if ( ! oriel_cs_260_command( cmd, Max_Grating_Switch_Delay ) )
    {
        print( FATAL, "Failed to switch to grating #%d.\n", grating + 1 );
        THROW( EXCEPTION );
    }

    oriel_cs_260.grating = oriel_cs_260_get_grating( );
    oriel_cs_260_get_wavelength( );
    oriel_cs_260_get_position( );

    return oriel_cs_260.grating;
}


/*--------------------------------*
 * Returns the number of lines of a grating (in lines/mm).
 * The argument is the grating number, 0 for garting #1
 * and 1 for grating #2.
 *--------------------------------*/

static
unsigned int
oriel_cs_260_get_number_of_lines( int grating )
{
    char req[ ] = "GRAT*LINES?\n";
    char reply[ 100 ];
    unsigned long val;
    char * ep;


    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );

    req[ 4 ] = grating + '1';
    if (    oriel_cs_260_talk( req, reply, sizeof reply, UNSET ) < 1
         || ! isdigit( ( int ) *reply ) )
        oriel_cs_260_failure( );

    val = strtoul( reply, &ep, 10 );
    if ( *ep || val == 0 || val == ULONG_MAX )
        oriel_cs_260_failure( );

    return oriel_cs_260.num_lines[ grating ] = val;
}


/*--------------------------------*
 * Set the number of lines of a grating. The first argument
 * is the grating number, 0 for garting #1 and 1 for grating
 * #2, the second the number of lines (per mm)
 *--------------------------------*/

static
unsigned int
oriel_cs_260_set_number_of_lines( int          grating,
                                  unsigned int num_lines )
{
    char cmd[ 100 ];

    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );
    fsc2_assert( num_lines > 0 );

    if ( oriel_cs_260.num_lines[ grating ] == num_lines )
        return num_lines;

    sprintf( cmd, "GRAT%dLINES %u\n", grating + 1, num_lines );
    if ( ! oriel_cs_260_command( cmd, 0 ) )
    {
        print( FATAL, "Failed to set number of lines to $f per m for grating "
               "#%d.\n", 1.0e-3 * num_lines, grating + 1 );
        THROW( EXCEPTION );
    }

    return oriel_cs_260.num_lines[ grating ] = num_lines;
}


/*--------------------------------*
 *--------------------------------*/

static
void
oriel_cs_260_calibrate( double wl )
{
    char cmd[ 100 ];

    sprintf( cmd, "CALIBRATE %.3f\n", wl );
    if ( ! oriel_cs_260_command( cmd, 0 ) )
    {
        print( FATAL, "Calibrate command failed.\n" );
        THROW( EXCEPTION );
    }
}


/*--------------------------------*
 * Returns the calibration factor for the given grating.
 * The argument is the grating number, 0 for garting #1
 * and 1 for grating #2.
 *--------------------------------*/

static double
oriel_cs_260_get_calibration_factor( int grating )
{
    char req[ ] = "GRAT*FACTOR?\n";
    char reply[ 100 ];
    double val;
    char * ep;


    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );

    req[ 4 ] = grating + '1';
    if (    oriel_cs_260_talk( req, reply, sizeof reply, UNSET ) < 1
         || ! isdigit( ( int ) reply[ 0 ] ) )
        oriel_cs_260_failure( );

    val = strtod( reply, &ep );
    if ( *ep || val == 0 || val == HUGE_VAL )
        oriel_cs_260_failure( );

    return oriel_cs_260.factor[ grating ] = val;
}


/*--------------------------------*
 * Set a new calibration factor for the given grating.
 * First argument is the grating number, 0 for garting
 * #1 and 1 for #2, second argument the new factor.
 *--------------------------------*/

static
double
oriel_cs_260_set_calibration_factor( int    grating,
                                     double factor )
{
    char cmd[ 100 ];

    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );
    fsc2_assert( factor >= 1.0e-6 );

    if ( factor == oriel_cs_260.factor[ grating ] )
        return factor;

    sprintf( cmd, "GRAT%dFACTOR %.6f\n", grating + 1, factor );
    if ( ! oriel_cs_260_command( cmd, 0 ) )
    {
        print( FATAL, "Failed to set calibration factor to %f for grating "
               "#%d.\n", factor, grating + 1 );
    }

    return oriel_cs_260.factor[ grating ] = factor;
}


/*--------------------------------*
 * Returns the calibration offset for the given grating.
 * The argument is the grating number, 0 for garting #1
 * and 1 for grating #2.
 *--------------------------------*/

static
double
oriel_cs_260_get_calibration_offset( int grating )
{
    char req[ ] = "GRAT*OFFSTE?\n";
    char reply[ 100 ];
    double val;
    char * ep;


    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );

    req[ 4 ] = grating + '1';
    if (    oriel_cs_260_talk( req, reply, sizeof reply, UNSET ) < 1
         || (    *reply != '-'
              && *reply != '+'
              && ! isdigit( ( int ) *reply ) ) )
        oriel_cs_260_failure( );

    val = strtod( reply, &ep );
    if ( *ep || val == - HUGE_VAL || val == HUGE_VAL )
        oriel_cs_260_failure( );

    return oriel_cs_260.offset[ grating ] = val;
}


/*--------------------------------*
 * Set a new calibration offset for the given grating.
 * First argument is the grating number, 0 for garting #1
 * and 1 for #2, second argument the new offset.
 *--------------------------------*/

static
double
oriel_cs_260_set_calibration_offset( int    grating,
                                     double offset )
{
    char cmd[ 100 ];


    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );

    if ( offset == oriel_cs_260.offset[ grating ] )
        return offset;

    sprintf( cmd, "GRAT%dOFFSET %.6f\n", grating + 1, offset );
    if ( ! oriel_cs_260_command( cmd, 0 ) )
    {
        print( FATAL, "Failed to sety calibration offset to %f for grating "
               "#%d.\n", offset, grating + 1 );
        THROW( EXCEPTION );
    }

    return oriel_cs_260.offset[ grating ] = offset;
}


/*--------------------------------*
 * Sets a gratings zero value.
 *--------------------------------*/

static
double
oriel_cs_260_get_zero( int    grating )
{
    char cmd[ ] = "GRAT*ZERO?\n";
    char reply[ 100 ];
    double zero;
    char *ep;


    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );

    cmd[ 4 ] = grating + '1';
    
    if ( oriel_cs_260_talk( cmd, reply, sizeof reply, UNSET ) < 1 )
        oriel_cs_260_failure( );

    zero = strtod( reply, &ep );
    if ( *ep || zero < 0 || zero == HUGE_VAL )
        oriel_cs_260_failure( );

    return oriel_cs_260.zero[ grating ] = zero;
}


/*--------------------------------*
 * Sets a gratings zero value.
 *--------------------------------*/

static
void
oriel_cs_260_set_zero( int    grating,
                       double zero )
{
    char buf[ 100 ];


    fsc2_assert( grating >= 0 && grating < NUM_GRATINGS );
    fsc2_assert( zero >= 0 );

    sprintf( buf, "GRAT%dZERO %.7f\n", grating + 1, zero );
    if ( ! oriel_cs_260_command( buf, 0 ) )
    {
        print( FATAL, "Failed to set grating zero to %f for grating #%d.\n",
               zero, grating + 1 );
        THROW( EXCEPTION );
    }

    oriel_cs_260.zero[ grating ] = zero;
}


/*--------------------------------*
 * Returns the eaveength the monocromator is currently set to.
 *--------------------------------*/

static double
oriel_cs_260_get_wavelength( void )
{
    char reply[ 100 ];
    double val;
    char *ep;


    if (    oriel_cs_260_talk( "WAVE?\n", reply, sizeof reply, UNSET ) < 1
         || ! isdigit( ( int ) reply[ 0 ] ) )
        oriel_cs_260_failure( );

    val = strtod( reply, &ep );
    if ( *ep || val == -HUGE_VAL || val == HUGE_VAL )
        oriel_cs_260_failure( );

    return oriel_cs_260.wavelength = 1.0e-9 * val;
}


/*--------------------------------*
 * Moves the monochromator to the given wavelength (or the
 * nearest possible one). Might thwo an exception if the
 * requested wavelength can't be set. Returns the exact
 * new wavelength as reported by the device.
 *--------------------------------*/

static
double
oriel_cs_260_set_wavelength( double wl )
{
    char cmd[ 100 ];


    fsc2_assert( wl >= 0 );

    sprintf( cmd, "GOWAVE %.3f\n", 1.0e9 * wl );

    /* Make sure we don't set a wavelnegth that's already set */

    wl = 1.0e-9 * strtod( cmd + 7, NULL );
    if ( wl == oriel_cs_260.wavelength )
        return oriel_cs_260.wavelength;

    if ( ! oriel_cs_260_command( cmd, Max_Wavelength_Delay ) )
    {
        if ( oriel_cs_260_get_error( ) == 3 )
            print( FATAL, "Can't set wavelength to %f nm, out of range.\n",
                   1.0e9 * wl );
        else
            print( FATAL, "Failed to set wavelength to %f nm.\n",
                   1.0e9 * wl );
        THROW( EXCEPTION );
    }

    /* Get the exact wavelength and stepper motor position from the device */

    oriel_cs_260_get_position( );
    return oriel_cs_260_get_wavelength( );
}


/*--------------------------------*
 * Returns the current stepper motor position"
 *--------------------------------*/

static
long int
oriel_cs_260_get_position( void )
{
    char reply[ 100 ];
    long pos;
    char *ep;


    if (    oriel_cs_260_talk( "STEP?\n", reply, sizeof reply, UNSET ) < 1
         || ! isdigit( ( int ) *reply ) )
        oriel_cs_260_failure( );

    pos = strtol( reply, &ep, 10 );
    if ( *ep )
        oriel_cs_260_failure( );

    return oriel_cs_260.position = pos;
}


/*--------------------------------*
 * Moves the monochromator by the given number of steps
 *--------------------------------*/

static
bool
oriel_cs_260_do_step( long int step )
{
    char cmd[ 100 ];


    if ( step == 0 )
        return SET;

    sprintf( cmd, "STEP %ld\n", step );
    if ( ! oriel_cs_260_command( cmd, Max_Wavelength_Delay ) )
    {
        if ( oriel_cs_260_get_error( ) == 2 )
        {
            print( WARN, "Can't change position by %ld, out of range.\n",
                   step );
            return UNSET;
        }
        else
            print( FATAL, "Failed to change position by %ld.\n", step );
        THROW( EXCEPTION );
    }

    oriel_cs_260.position += step;
    oriel_cs_260_get_wavelength( );
    return SET;
}


/*--------------------------------*
 * Returns if the shutter is closed
 *--------------------------------*/

static
bool
oriel_cs_260_get_shutter( void )
{
    char reply[ 3 ];


    if (    oriel_cs_260_talk( "SHUTTER?\n", reply, sizeof reply, SET ) != 1
         || ( *reply != 'C' && *reply != 'O' ) )
        oriel_cs_260_failure( );

    return oriel_cs_260.shutter_state = *reply == 'C';
}


/*--------------------------------*
 * Opens (if called with a 'false' value) or closes
 *  (if called with a 'true' value) the shutter
 *--------------------------------*/

static
bool
oriel_cs_260_set_shutter( bool on_off )
{
    char cmd[ ] = "SHUTTER *\n";
    struct timeval now;


    if ( on_off == oriel_cs_260.shutter_state )
        return on_off;

    /* Avoid opening and closing the shutter more quickly than allowed,
       it needs to stay open for at least 0.2 seconds and shouldn't be
       opened and closed with a rate of more than 0.5 Hz */

    gettimeofday( &now, NULL );
    if ( on_off )
    {
        double diff =   ( now.tv_sec  + 1.0e-6 * now.tv_usec  )
                      - (   oriel_cs_260.last_shutter_close.tv_sec
                          + 1.0e-6 * oriel_cs_260.last_shutter_close.tv_usec );
        if ( diff < MIN_SHUTTER_REPEAT_TIME )
            fsc2_usleep( 1.0e6 * ( MIN_SHUTTER_REPEAT_TIME - diff ), UNSET );
    }
    else
    {
        double diff =   ( now.tv_sec  + 1.0e-6 * now.tv_usec  )
                      - (   oriel_cs_260.last_shutter_open.tv_sec
                          + 1.0e-6 * oriel_cs_260.last_shutter_open.tv_usec );
        if ( diff < MIN_SHUTTER_EXPOSURE_TIME )
            fsc2_usleep( 1.0e6 * ( MIN_SHUTTER_EXPOSURE_TIME - diff ), UNSET );
    }

    cmd[ 8 ] = on_off ? 'C' : 'O';
    if ( ! oriel_cs_260_command( cmd, Max_Shutter_Delay ) )
    {
        print( FATAL, "Failed to %s shutter.\n", on_off ? "close" : "open" );
        THROW( EXCEPTION );
    }

    if ( on_off )
        gettimeofday( &oriel_cs_260.last_shutter_open, NULL );
    else
        gettimeofday( &oriel_cs_260.last_shutter_close, NULL );

	return oriel_cs_260.shutter_state = on_off;
}


/*----------------------------------------------------*
 * Returns the number of the current filter, a number
 * between 0 and 5, or -1 if the filter wheel is out
 * of position.
 *----------------------------------------------------*/

#if defined HAS_FILTER_WHEEL
static
int
oriel_cs_260_get_filter( void )
{
    char reply[ 3 ];


    if (    oriel_cs_260_talk( "FILTER?\n", reply, sizeof reply, SET ) != 1
         || ! isdigit( ( int ) *reply ) )
        oriel_cs_260_failure( );

    /* If '0' got returned the filter wheel is out of position (or there
       isn't one) and we need to reset the error byte by reading it */

    if ( *reply == '0' )
    {
        if ( oriel_cs_260_get_error( ) == 6 )
            print( FATAL, "Device has no filter wheel.\n" );
        else
            print( FATAL, "Filter wheel is out of position.\n" );
        THROW( EXCEPTION );
    }
    else if ( reply[ 0 ] - '0' > NUM_FILTERS )
        oriel_cs_260_failure( );

    return oriel_cs_260.filter = reply[ 0 ] - '1';
}
#endif


/*----------------------------------------------------*
 * Switches to a new filter and waits for it to be in position.
 *----------------------------------------------------*/

#if defined HAS_FILTER_WHEEL
static
int
oriel_cs_260_set_filter( int filter )
{
    char cmd[ ] = "FILTER *\n";

    fsc2_assert( filter >= 0 && filter < NUM_FILTERS );

    if ( filter == oriel_cs_260.filter )
        return filter;

    cmd[ 7 ] = '1' + filter + '1';
    if ( ! oriel_cs_260_command( cmd, Max_Filter_Delay ) )
    {
        print( FATAL, "Failed to switch to filter #%d.\n", filter + 1 );
        THROW( EXCEPTION );
    }

    return oriel_cs_260.filter = filter;
}
#endif


/*--------------------------------------------------*
 *--------------------------------------------------*/

static
int
oriel_cs_260_get_outport( void )
{
    char reply[ 3 ];

    if (    oriel_cs_260_talk( "OUTPORT?\n", reply, sizeof reply, SET ) != 1
         || ( *reply != '1' && *reply != '2' ) )
        oriel_cs_260_failure( );

    return oriel_cs_260.outport = *reply - '1';
}


int
oriel_cs_260_set_outport( int outport )
{
    char cmd[ ] = "OUTPORT *\n";

    fsc2_assert( outport == 0 || outport == 1 );

    cmd[ 8 ] = '1' + outport + '1';
    if ( ! oriel_cs_260_command( cmd, Max_Outport_Switch_Delay ) )
    {
        print( FATAL, "Failed to switch to %s output port.\n",
               outport ? "lateral" : "axial" );
        THROW( EXCEPTION );
    }

    return oriel_cs_260.outport = outport;
}
    

/*--------------------------------------------------*
 * Requests the error state. Returns -1 if there was no
 * error, otherwise the error code as documented in the
 * manual.
 *--------------------------------------------------*/

static
int
oriel_cs_260_get_error( void )
{
    char reply[ 3 ];
    long len = oriel_cs_260_talk( "ERROR?\n", reply, sizeof reply, UNSET );


    if ( len == 0 )     /* no error */
        return -1;

    if (    len != 1
         || ! isdigit( ( int ) *reply )
         || ( *reply > '3' && ! ( *reply >= '6' && *reply <= 9 ) ) )
        oriel_cs_260_failure( );

    return reply[ 0 ] - '0';
}


/*--------------------------------*
 * Sends a message to the device, not expecting a reply but just
 * a handshake within 'wait_for' seconds
 *--------------------------------*/

static
bool
oriel_cs_260_command( const char * cmd,
                      double       wait_for )
{
    struct timeval before,
                   after;
    char buf[ 3 ];
    long len = 3;

    
    /* Try to send the command */

    if ( gpib_write( oriel_cs_260.device, cmd, strlen( cmd ) ) == FAILURE )
        oriel_cs_260_failure( );

    /* Now try to read the acknowledgment ("handshake). On success it will
       be just the string "0\r\n", otherwise "32\r\n". Te acknowledgment
       may take some time for commands like switcing gratings or for
       large wavelength changes. Thus repeat attempts to read until
       the timeout, givem in seconds via 'wait_for', is over before
       giving up. */

    while ( 1 )
    {
        len = 3;
        gettimeofday( &before, NULL );

        if ( gpib_read( oriel_cs_260.device, buf, &len ) == SUCCESS )
            break;

        gettimeofday( &after, NULL );
            
        wait_for -=   ( after.tv_sec  + 1.0e-6 * after.tv_usec  )
                    - ( before.tv_sec + 1.0e-6 * before.tv_usec );

        if ( wait_for <= 0 )
            oriel_cs_260_failure( );

        before = after;
    }

    if ( ! strcmp( buf, "0\r\n" ) )
    {
        fprintf( stderr, "Got andshake\n" );
        return SET;
    }

    len = 1;
    if ( gpib_read( oriel_cs_260.device, buf, &len ) == FAILURE )
        oriel_cs_260_failure( );

    return UNSET;
}


/*-------------------------------------*
 * Sends a message and returns the reply (after testing for and removing
 * the trailing '\r' and '\n') and its length
 *-------------------------------------*/

static
long
oriel_cs_260_talk( const char * cmd,
                   char       * reply,
                   long         length,
                   bool         exact_length )
{
    char buf[ 3 ];
    long hs_len = 3;

    if ( gpib_write( oriel_cs_260.device, cmd, strlen( cmd ) ) == FAILURE )
        oriel_cs_260_failure( );

    /* Something strange is going on with this device: despite we have set
       that reading should stop after encountering a line-feed (which gets
       sent by the device at the end of each message) this doesn't get
       recognized and a timeout ensues. Thus we treat this the follwoing way:
       if the caller signals that it knows the exact size of the reply to
       be expected we call gpib_read() with this size and cross fingers.
       Otherwise we read byte by byte and check each time for the line-feed.
       Not beautiful, but if it works... */

    if ( exact_length )
    {
        if ( gpib_read( oriel_cs_260.device, reply, &length ) == FAILURE )
            oriel_cs_260_failure( );
    }
    else
    {
        long len = 0;

        while ( len < length )
        {
            long x = 1;

            if ( gpib_read( oriel_cs_260.device, reply + len, &x ) == FAILURE )
                oriel_cs_260_failure( );
            if ( reply[ len++ ] == '\n' )
                break;
        }

        length = len;
    }

    if (    reply[ length - 2 ] != '\r'
         || reply[ length - 1 ] != '\n' )
        oriel_cs_260_failure( );

    reply[ length -= 2 ] = '\0';

    /* Finally, read the "handshake". It always should be the positive reply
       "0\r\n" - or something is seriously weird. */

    if (    gpib_read( oriel_cs_260.device, buf, &hs_len ) == FAILURE
         || ! strncmp( buf, "0\r\n", 3 ) )
        oriel_cs_260_failure( );

    return length;
}


/*-----------------------------------------*
 * Returns the currently used GPIB address
 *-----------------------------------------*/

static
int
oriel_cs_260_get_gpib_address( void )
{
    char reply[ 100 ];
    unsigned long addr;
    char *ep;


    if (    oriel_cs_260_talk( "ADDRESS?\n", reply, sizeof reply, UNSET ) < 1
         || ! isdigit( ( int ) reply[ 0 ] ) )
        oriel_cs_260_failure( );

    addr = strtoul( reply, &ep, 10 );
    if ( *ep || addr > 30 )
        oriel_cs_260_failure( );

    return addr;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static void
oriel_cs_260_failure( void )
{
    print( FATAL, "Communication with monochromator failed.\n" );
    THROW( EXCEPTION );
}


/*-----------------------------------------*
 * Sets a new GPIB address. That's the only way to change it and
 * when this has been done we can't continue talking to the device...
 *-----------------------------------------*/

static
void
oriel_cs_260_set_gpib_address( unsigned int addr )
{
    char cmd[ 20 ];


    fsc2_assert( addr <= 30 );

    if ( ( int ) addr == oriel_cs_260_get_gpib_address( ) )
        return;

    sprintf( cmd, "ADDRESS %2u\n", addr );
    if ( gpib_write( oriel_cs_260.device, cmd, strlen( cmd ) ) == FAILURE )
        oriel_cs_260_failure( );
}


/*-----------------------------------------*
 * Converts a wavelength into a wavenumber
 *-----------------------------------------*/

static
double
oriel_cs_260_wl2wn( double wl )
{
    fsc2_assert( wl != 0 );
    return 0.01 / wl;
}


/*-----------------------------------------*
 * Converts a wavenumber into a wavelength
 *-----------------------------------------*/

static
double
oriel_cs_260_wn2wl( double wn )
{
    fsc2_assert( wn != 0 );
    return 0.01 / wn;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
