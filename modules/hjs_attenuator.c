/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "hjs_attenuator.h"
#include "serial.h"


/* Global variables */


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

HJS_Attenuator_T hjs_attenuator;


/* Local variables */

static HJS_Attenuator_T hjs_attenuator_stored;


/* Local functions */

static bool hjs_attenuator_serial_init( void );

static void hjs_attenuator_set_attenuation( long new_step );

static void hjs_attenuator_comm_failure( void );


static FILE * hjs_attenuator_find_calibration( char ** name );

static FILE * hjs_attenuator_open_calibration( char * name );

static long hjs_attenuator_att_to_step( double att );


/*-------------------------------------------*/
/*                                           */
/*            Hook functions                 */
/*                                           */
/*-------------------------------------------*/

/*------------------------------------------------------*
 * Function called when the module has just been loaded
 *------------------------------------------------------*/

int
hjs_attenuator_init_hook( void )
{
    hjs_attenuator.sn = fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

    hjs_attenuator.is_open       = UNSET;
    hjs_attenuator.is_step       = UNSET;
    hjs_attenuator.calib_file    = NULL;
    hjs_attenuator.att_table     = NULL;
    hjs_attenuator.att_table_len = 0;

    vars_pop( mw_attenuator_load_calibration( NULL ) );

    return 1;
}


/*----------------------------------------------*
 * Function called at the start of the test run
 *----------------------------------------------*/

int
hjs_attenuator_test_hook( void )
{
    hjs_attenuator_stored = hjs_attenuator;
    return 1;
}


/*-----------------------------------------------*
 * Function called at the start of an experiment
 *-----------------------------------------------*/

int
hjs_attenuator_exp_hook( void )
{
    /* Restore state from before the start of the test run (or the
       last experiment) */

    hjs_attenuator = hjs_attenuator_stored;

    if ( ! ( hjs_attenuator.is_open = hjs_attenuator_serial_init( ) ) )
        hjs_attenuator_comm_failure( );
    
    return 1;
}


/*---------------------------------------------*
 * Function called at the end of an experiment
 *---------------------------------------------*/

int
hjs_attenuator_end_of_exp_hook( void )
{
    if ( hjs_attenuator.is_open )
    {
        fsc2_serial_close( hjs_attenuator.sn );
        hjs_attenuator.is_open = UNSET;
    }

    return 1;
}


/*------------------------------------------------------*
 * Function called just before the module gets unloaded
 *------------------------------------------------------*/

void
hjs_attenuator_exit_hook( void )
{
    /* Get rid of the data from the calibration file */

    if ( hjs_attenuator.att_table )
        hjs_attenuator.att_table = T_free( hjs_attenuator.att_table );
}


/*--------------------------------------------------------------------*
 * Function called at the very end of the experiment within the child
 * process. It set the attenuator to the maximum attenuation.
 *--------------------------------------------------------------------*/

void
hjs_attenuator_child_exit_hook( void )
{
    if ( hjs_attenuator.is_step )
        hjs_attenuator_set_attenuation(
                lrnd( hjs_attenuator.att_table[ hjs_attenuator.att_table_len
                                                - 1 ].step ) );
}


/*-------------------------------------------*/
/*                                           */
/*             EDL functions                 */
/*                                           */
/*-------------------------------------------*/

/*----------------------------------------------------------------*
 * Function returns a string variable with the name of the device
 *----------------------------------------------------------------*/

Var_T *
mw_attenuator_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*-----------------------------------------------------------*
 * Function allows to load a new list of attenuations versus
 * stepper motor positions from a file. When called without
 * an argument the default calibration file is loaded, which
 * is defined in the configuration file for the module.
 * The default calibration file is loaded immediately when
 * the module got loaded, so it's not necessary to call the
 * function unless a different calibration file is to be
 * used.
 *-----------------------------------------------------------*/

Var_T *
mw_attenuator_load_calibration( Var_T * v )
{
    FILE *tfp = NULL;


    CLOBBER_PROTECT( tfp );

    if ( hjs_attenuator.is_step )
    {
        print( FATAL, "calibration file can only be loaded before an "
               "attenuation has been set.\n" );
        THROW( EXCEPTION );
    }

    /* If a table had already been read in get rid of it */

    if ( hjs_attenuator.att_table != NULL )
    {
        hjs_attenuator.att_table = T_free( hjs_attenuator.att_table );
        hjs_attenuator.att_table_len = 0;
    }

    /* Try to figure out the name of the calibration file - if no argument is
       given use the default calibration file, otherwise use the user supplied
       file name */

    if ( v == NULL )
    {
        if ( DEFAULT_CALIB_FILE[ 0 ] ==  '/' )
            hjs_attenuator.calib_file = T_strdup( DEFAULT_CALIB_FILE );
        else
            hjs_attenuator.calib_file = get_string( "%s%s%s", libdir,
                                                    slash( libdir ),
                                                    DEFAULT_CALIB_FILE );

        if ( ( tfp = 
               hjs_attenuator_open_calibration( hjs_attenuator.calib_file ) )
             == NULL )
        {
            print( FATAL, "Calibration file '%s' not found.\n",
                   hjs_attenuator.calib_file );
            hjs_attenuator.calib_file = T_free( hjs_attenuator.calib_file );
            THROW( EXCEPTION );
        }
    }
    else
    {
        char *tfname;

        CLOBBER_PROTECT( tfname );

        vars_check( v, STR_VAR );
        tfname = T_strdup( v->val.sptr );
        too_many_arguments( v );

        TRY
        {
            tfp = hjs_attenuator_find_calibration( &tfname );
            hjs_attenuator.calib_file = tfname;
            TRY_SUCCESS;
        }
        CATCH( EXCEPTION )
        {
            T_free( tfname );
            RETHROW( );
        }
    }

    /* Now try to read in the table file */

    TRY
    {
        hjs_attenuator_read_calibration( tfp );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        fclose( tfp );
        hjs_attenuator.calib_file = T_free( hjs_attenuator.calib_file );
        RETHROW( );
    }

    fclose( tfp );
    hjs_attenuator.calib_file = T_free( hjs_attenuator.calib_file );

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------------------------*
 * Function that must be called before any other function to set or get
 * the current attenuation, and then never again. It informs the module
 * about the attenuation currently set. This information is required to
 * calculate the number of steps needed to change to a new attenuation.
 *----------------------------------------------------------------------*/

Var_T *
mw_attenuator_initial_attenuation( Var_T * v )
{
    if ( hjs_attenuator.is_step )
    {
        if ( FSC2_MODE == EXPERIMENT )
        {
            print( SEVERE, "Initial attenuation already has been set, "
                   "discarding new value.\n" );
            return vars_push( FLOAT_VAR, hjs_attenuator.att );
        }

        print( FATAL, "Initial attenuation already has been set.\n" );
        THROW( EXCEPTION );
    }

    hjs_attenuator.att = get_double( v, "attenuation" );
    hjs_attenuator.step = hjs_attenuator_att_to_step( hjs_attenuator.att );
    hjs_attenuator.is_step = SET;
    return vars_push( FLOAT_VAR, hjs_attenuator.att );
}


/*----------------------------------------------------------------*
 * EDL function for querying or setting the attenuation. Calling
 * the function requires that mw_attenuator_initial_attenuation()
 * has been called before to tell the module about the initial
 * attenuation set at the device.
 *----------------------------------------------------------------*/

Var_T *
mw_attenuator_attenuation( Var_T * v )
{
    double att;
    long step;


    if ( ! hjs_attenuator.is_step )
    {
        print( FATAL, "Don't know the current attenuation setting. Always "
               "call 'mw_attenuator_initial_attenuation()' first!\n" );
        THROW( EXCEPTION );
    }

    if ( v != NULL )
    {
        att = get_double( v, "attenuation" );
        step = hjs_attenuator_att_to_step( att );

        if ( FSC2_MODE == EXPERIMENT )
            hjs_attenuator_set_attenuation( step );

        if ( step - hjs_attenuator.step != 0 )
        {
            hjs_attenuator.att = att;
            hjs_attenuator.step = step;
        }
    }

    return vars_push( FLOAT_VAR, hjs_attenuator.att );
}


/*-------------------------------------------*/
/*                                           */
/*           Low level functions             */
/*                                           */
/*-------------------------------------------*/

/*---------------------------------------------------------------*
 * Initialization of the serial port the device is connected to.
 *---------------------------------------------------------------*/

static bool
hjs_attenuator_serial_init( void )
{
    if ( ( hjs_attenuator.tio = fsc2_serial_open( hjs_attenuator.sn,
                                 O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) )
                                                                      == NULL )
        return FAIL;

    /* The device uses 6 bit transfers, no parity and one stop bit (6-N-1),
       and a baud rate of 9600. The settings used here are the ones that
       one gets when opening the device file with fopen() and these seem
       just to be settings that work with the device (there's no manual...) */

    memset( hjs_attenuator.tio, 0, sizeof *hjs_attenuator.tio );

    hjs_attenuator.tio->c_iflag = ICRNL | IXON;
    hjs_attenuator.tio->c_oflag = OPOST | ONLCR;
    hjs_attenuator.tio->c_cflag = HUPCL | CSIZE| CS6 | CLOCAL;
    hjs_attenuator.tio->c_lflag = ISIG | ICANON | ECHONL | TOSTOP | IEXTEN;

    cfsetispeed( hjs_attenuator.tio, SERIAL_BAUDRATE );
    cfsetospeed( hjs_attenuator.tio, SERIAL_BAUDRATE );

    fsc2_tcflush( hjs_attenuator.sn, TCIOFLUSH );
    fsc2_tcsetattr( hjs_attenuator.sn, TCSANOW, hjs_attenuator.tio );

    return OK;
}


/*-----------------------------------------------------------------------*
 * Function for setting the new attenuation by moving the stepper motor.
 *-----------------------------------------------------------------------*/

static void
hjs_attenuator_set_attenuation( long new_step )
{
    long steps;
    char *cmd;
    ssize_t len;


    /* Figure out how many steps to go */

    steps = new_step - hjs_attenuator.step;

    if ( steps == 0 )
        return;

    /* Assemble the command ans send it to the device */

    cmd = get_string( "@01\n@0A %+ld,300\n", steps );
    len = ( ssize_t ) strlen( cmd );

    if ( fsc2_serial_write( hjs_attenuator.sn, cmd, ( size_t ) len,
                            100000L, UNSET ) != len )
    {
        T_free( cmd );
        hjs_attenuator_comm_failure( );
    }

    T_free( cmd );

    /* Wait for the motor to move, we're at a speed of 300 steps per second. */

    fsc2_usleep( ulrnd( fabs( steps / 300.0 * 1000000L ) ), UNSET );

    /* To always reach the end point from the same side we go a bit further
       up when we come from below and then back down again (obviously, the
       device allows to go a bit (50 steps) over the upper limit of steps
       given in the calibration file). */

    if ( steps < 0 )
        return;

    if ( fsc2_serial_write( hjs_attenuator.sn, "@01\n@0A +50,100\n", 16,
                            100000L, UNSET ) != 16 )
        hjs_attenuator_comm_failure( );

    fsc2_usleep( 500000, UNSET );

    if ( fsc2_serial_write( hjs_attenuator.sn, "@01\n@0A -50,300\n", 16,
                            100000L, UNSET ) != 16 )
        hjs_attenuator_comm_failure( );

    fsc2_usleep( 166667, UNSET );
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

static void
hjs_attenuator_comm_failure( void )
{
    print( FATAL, "Can't access the attenuator.\n" );
    THROW( EXCEPTION );
}


/*--------------------------------------------------------------------------*
 * If the function succeeds it returns a file pointer to the calibration
 * file and sets the calib_file entry in the device structure to the name
 * of the calibration file. Otherwise an exception is thrown. In any case
 * the memory used for the file name passed to the function is deallocated.
 *--------------------------------------------------------------------------*/

static FILE *
hjs_attenuator_find_calibration( char ** name )
{
    FILE *tfp;
    char *new_name;


    /* Expand a leading tilde to the users home directory */

    if ( ( *name )[ 0 ] == '~' )
    {
        new_name = get_string( "%s%s%s", getenv( "HOME" ),
                               ( *name )[ 1 ] != '/' ? "/" : "", *name + 1 );
        T_free( *name );
        *name = new_name;
    }

    /* Now try to open the file - set calib_file entry in the device structure
       to the name and return the file pointer */

    if ( ( tfp = hjs_attenuator_open_calibration( *name ) ) != NULL )
    {
        hjs_attenuator.calib_file = T_strdup( *name );
        return tfp;
    }

    /* If the file name contains a slash we give up after freeing memory */

    if ( strchr( *name, '/' ) != NULL )
    {
        print( FATAL, "Calibration file '%s' not found.\n", *name );
        THROW( EXCEPTION );
    }

    /* Last chance: The calibration file is in the library directory... */

    new_name = get_string( "%s%s%s", libdir, slash( libdir ), *name );
    T_free( *name );
    *name = new_name;

    if ( ( tfp = hjs_attenuator_open_calibration( *name ) ) == NULL )
    {
        print( FATAL, "Calibration file '%s' not found in either the current "
               "directory or in '%s'.\n", strip_path( *name ), libdir );
        THROW( EXCEPTION );
    }

    return tfp;
}


/*------------------------------------------------------------------*
 * Tries to open the file with the given name and returns the file
 * pointer, returns NULL if file does not exist returns, or throws
 * an exception if the file can't be read (either because of prob-
 * lems with the permissions or other, unknoen reasons). If opening
 * the file fails with an exception memory used for its name is
 * deallocated.
 *------------------------------------------------------------------*/

static FILE *
hjs_attenuator_open_calibration( char * name )
{
    FILE *tfp;


    if ( access( name, R_OK ) == -1 )
    {
        if ( errno == ENOENT )       /* file not found */
            return NULL;

        print( FATAL, "No read permission for calibration file '%s'.\n",
               name );
        T_free( name );
        THROW( EXCEPTION );
    }

    if ( ( tfp = fopen( name, "r" ) ) == NULL )
    {
        print( FATAL, "Can't open calibration file '%s'.\n", name );
        T_free( name );
        THROW( EXCEPTION );
    }

    return tfp;
}


/*--------------------------------------------------------------------*
 * Function for determining the stepper motor position for a certain
 * attenuation. The attenuation can't be negative and must be between
 * the minimum and maximum attenuation in the array of attenuation/
 * motor position settings in the array read in from the calibration
 * file. For attenutaions where there is no entry in the list linear
 * interpolation (and rounded to the next integer position) between
 * the neighboring entries is used.
 *--------------------------------------------------------------------*/

static long
hjs_attenuator_att_to_step( double att )
{
    size_t ind;
    size_t max_index;;


    fsc2_assert(    hjs_attenuator.att_table != NULL
                 && hjs_attenuator.att_table_len > 1 );

    if ( att < 0.0 )
    {
        print( FATAL, "Invalid negative attenuation of %.1f dB.\n", att );
        THROW( EXCEPTION );
    }

    /* Check that the attenuation value is covered by the list, give it
       a bit of slack (0.1%) for rounding errors */

    if ( att > hjs_attenuator.max_table_att * 1.001 )
    {
        print( FATAL, "Attenuation of %.1f dB is too large, must not be "
               "larger than %.1f dB.\n", att, hjs_attenuator.max_table_att );
        THROW( EXCEPTION );
    }

    if ( att >= hjs_attenuator.max_table_att )
        return lrnd( hjs_attenuator.att_table[ hjs_attenuator.att_table_len
                                               - 1 ].step );

    if ( att < hjs_attenuator.min_table_att * 0.999 )
    {
        print( FATAL, "Attenuation of %.1f dB is too small, must not be "
               "smaller than %.1f dB.\n", att, hjs_attenuator.min_table_att );
        THROW( EXCEPTION );
    }

    if ( att <= hjs_attenuator.min_table_att )
        return lrnd( hjs_attenuator.att_table[ 0 ].step );

    /* Do a binary search of the list for the attenuation, interpolate if
       necesaary */

    ind = hjs_attenuator.att_table_len / 2;
    max_index = hjs_attenuator.att_table_len - 1;

    while( 1 )
    {
        if ( att < hjs_attenuator.att_table[ ind ].att )
        {
            if ( att >= hjs_attenuator.att_table[ ind - 1 ].att )
                return lrnd( hjs_attenuator.att_table[ ind - 1 ].step +
                           ( hjs_attenuator.att_table[ ind - 1 ].step
                             -  hjs_attenuator.att_table[ ind ].step ) /
                           ( hjs_attenuator.att_table[ ind - 1 ].att
                             -  hjs_attenuator.att_table[ ind ].att ) *
                           ( att - hjs_attenuator.att_table[ ind - 1 ].att ) );
                    
            max_index = ind;
            ind /= 2;
            continue;
        }

        if ( att < hjs_attenuator.att_table[ ind + 1 ].att )
                return lrnd( hjs_attenuator.att_table[ ind ].step +
                             ( hjs_attenuator.att_table[ ind + 1 ].step
                               -  hjs_attenuator.att_table[ ind ].step ) /
                             ( hjs_attenuator.att_table[ ind + 1 ].att
                               -  hjs_attenuator.att_table[ ind ].att ) *
                             ( att - hjs_attenuator.att_table[ ind ].att ) );

        ind += ( max_index - ind ) / 2;
    }

    return 0;       /* we can't ever end up here */
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
