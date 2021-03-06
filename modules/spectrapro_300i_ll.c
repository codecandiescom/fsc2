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


#include "spectrapro_300i.h"


#define SPECTRAPRO_300I_WAIT  100000   /* 100 ms */


enum {
       SERIAL_INIT,
       SERIAL_EXIT,
       SERIAL_READ,
       SERIAL_WRITE
};


/* List of all "officially supported" gratings - others exist but trying to
   install them simply using the INSTALL command results in the device getting
   into a garbled state. Instead a similar grating with the same number of
   grooves per millimeter has to be "installed" and then the blaze wavelength
   to be corrected (actually, the blaze wavelength is irrelevant for the
   performance of the device as far as I was told by tech support). */

static const char *good_gratings[ ] = { "1-002-45",
                                        "1-050-12"
                                        "1-075-8",
                                        "1-015-500",
                                        "1-015-4",
                                        "1-030-300",
                                        "1-030-2",
                                        "1-060-300",
                                        "1-060-1",
                                        "1-060-1.6",
                                        "1-120-300",
                                        "1-120-500",
                                        "1-120-750",
                                        "1-120-HUV",
                                        "1-120-HVIS",
                                        "1-180-250",
                                        "1-180-500",
                                        "1-240-240",
                                        "1-240-H",
                                        "1-360-240" };


static void spectrapro_300i_send( const char * buf );

static bool spectrapro_300i_read( char *   buf,
                                  size_t * len );

static char * spectrapro_300i_talk( const char * buf,
                                    size_t       len );

static bool spectrapro_300i_comm( int type,
                                  ... );

static void spectrapro_300i_comm_fail( void );


/*-----------------------------------------------------------*
 * If the function succeeds it returns a file pointer to the
 * calibration file. Otherwise an exception is thrown.
 *-----------------------------------------------------------*/

FILE *
spectrapro_300i_find_calib( char * name )
{
    FILE *cfp = NULL;
    char * volatile new_name = NULL;


    /* Expand a leading tilde to the users home directory */

    if ( name[ 0 ] == '~' )
        new_name = get_string( "%s%s%s", getenv( "HOME" ),
                               name[ 1 ] != '/' ? "/" : "", name + 1 );

    /* Now try to open the file and return the file pointer */

    TRY
    {
        cfp = spectrapro_300i_open_calib( new_name != NULL ?
                                          new_name : name );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( new_name );
        RETHROW;
    }

    if ( new_name != NULL )
        T_free( new_name );

    if ( cfp )
        return cfp;

    /* If the file name contains a slash we give up after freeing memory */

    if ( strchr( name, '/' ) != NULL )
    {
        print( FATAL, "Calibration file '%s' not found.\n", new_name );
        THROW( EXCEPTION );
    }

    /* Last chance: The calibration file is in the library directory... */

    new_name = get_string( "%s%s%s", libdir, slash( libdir ), name );

    TRY
    {
        cfp = spectrapro_300i_open_calib( new_name );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( new_name );
        RETHROW;
    }

    if ( cfp == NULL )
    {
        print( FATAL, "Calibration file '%s' not found in either the current "
               "directory or in '%s'.\n", strip_path( new_name ), libdir );
        T_free( new_name );
        THROW( EXCEPTION );
    }

    T_free( new_name );

    return cfp;
}


/*------------------------------------------------------------------*
 * Tries to open the file with the given name and returns the file
 * pointer or NULL if file does not exist returns, or throws an
 * exception if the file can't be read (either because of problems
 *  with the permissions or other, unknown reasons).
 *------------------------------------------------------------------*/

FILE *
spectrapro_300i_open_calib( char * name )
{
    FILE *cfp;


    if ( access( name, R_OK ) == -1 )
    {
        if ( errno == ENOENT )       /* file not found */
            print( FATAL, "Calibration file '%s' not found.\n", name );
        else if ( errno == EACCES )
            print( FATAL, "No permission to read calibration file '%s'.\n",
                   name );
        else
            print( FATAL, "Can't access calibration file '%s'.\n", name );
        THROW( EXCEPTION );
    }

    if ( ( cfp = fopen( name, "r" ) ) == NULL )
    {
        print( FATAL, "Can't open calibration file '%s'.\n", name );
        THROW( EXCEPTION );
    }

    return cfp;
}


/*-----------------------------------------------------------------------*
 * Function for calculating the sum of the squares of the differences
 * between the measured offsets of the lines from he center (in pixels)
 * and the calculated offsets. The formulas used are (in TeX notation):
 * $n$: deviation in numbers of pixels
 * $x$: width of a single pixel
 * $m$: diffraction order
 * $\psi$: grating angle, rotation angle of the grating
 * $\gamma$: inclusion angle, angle between incoming and outgoing beam
 * $f$: focal length of monochromator
 * $\delta$: detector angle, deviation of angle between center beam and
 * perpendicular on the CCD plane
 * $\theta$: incident angle of beam on detector
 * $\lambda$: wavelength of measured line
 * $\lambda_C$: wavelength at the center of the detector
 * \begin{eqnarray*}
 * \tan \theta & = & \frac{ nx \, \cos \delta}{f + nx \, \sin \delta} \\
 * \sin \psi & = & \frac{m \lambda_C}{2 d \, \cos \gamma / 2}         \\
 * \lambda & = & \frac{d}{m} \, \{ \sin ( \psi - \frac{\gamma}{2} )
 *               + \sin ( \psi - \frac{\gamma}{2} + \theta ) \}
 * \end{eqnarray*}
 *-----------------------------------------------------------------------*/

double
spectrapro_300i_min( double * x,
                     void *   par )
{
    double inclusion_angle_2;                 /* half of inclusion angle */
    double focal_length;
    double detector_angle;
    double grating_angle;
    double sum = 0.0;
    double a, b;
    size_t i;
    Calib_Params_T *c = ( Calib_Params_T * ) par;


    if ( c->opt == 0 )
    {
        inclusion_angle_2 = 0.5 * x[ 0 ];
        if ( inclusion_angle_2 <= 0.0 )
            return HUGE_VAL;

        focal_length = c->focal_length;
        detector_angle = c->detector_angle;
    }
    else if ( c->opt == 1 )
    {
        focal_length = x[ 0 ];
        if ( focal_length <= 0.0 )
            return HUGE_VAL;

        inclusion_angle_2 = 0.5 * c->inclusion_angle;
        detector_angle    = c->detector_angle;
    }
    else if ( c->opt == 2 )
    {
        detector_angle    = x[ 0 ];

        inclusion_angle_2 = 0.5 * c->inclusion_angle;
        focal_length      = c->focal_length;
    }
    else
        THROW( EXCEPTION );

    /* For each measured value (that's the deviation of the line position
       from the center, expressed in pixels) calculate the theoretical value
       with the values for the inclusion angle, the focal length and the
       detector angle we got from the simplex routine. Then sum up the squares
       of the deviations from the measured values, which is what we want to
       minimize. */

    for ( i = 0; i < c->num_values; i++ )
    {
        a = c->m[ i ] * c->center_wavelengths[ i ]
            / ( 2.0 * c->d * cos( inclusion_angle_2 ) );

        if ( fabs( a ) > 1.0 )
        {
            sum = HUGE_VAL;
            break;
        }

        grating_angle = asin( a );

        a = c->m[ i ] * c->wavelengths[ i ] / c->d
            - sin( grating_angle - inclusion_angle_2 );

        if ( fabs( a ) > 1.0 )
        {
            sum = HUGE_VAL;
            break;
        }

        a = tan( asin( a ) - grating_angle - inclusion_angle_2 );

        b = cos( detector_angle ) - a * sin( detector_angle );

        if ( b == 0.0 )
        {
            sum = HUGE_VAL;
            break;
        }

        b = c->n_exp[ i ] - focal_length * a / ( b * c->pixel_width );

        sum += b * b;
    }

    return sum;
}


/*----------------------------------------------------------------*
 * Function tries to open the device file for the serial port the
 * monochromator is attached to, checks if it replies to commands
 * and then fetches some basic informations about the state of
 * the monochromator like which gratings are installed and the
 * current grating and turret.
 *----------------------------------------------------------------*/

void
spectrapro_300i_open( void )
{
    char reply[ 100 ];
    size_t len;
    size_t already_read = 0;
    int i;


    if ( ! spectrapro_300i_comm( SERIAL_INIT ) )
    {
        print( FATAL, "Can't open device file for monochromator.\n" );
        THROW( EXCEPTION );
    }

    spectrapro_300i.is_open = SET;

    /* Now a quick check that we can talk to the monochromator, it should be
       able to send us its model string within one second or something is
       definitely hosed... */

    if ( ! spectrapro_300i_comm( SERIAL_WRITE, "MODEL\r" ) )
        spectrapro_300i_comm_fail( );

    for ( i = 0; i < 10; i++ )
    {
        len = 100 - already_read;
        if ( spectrapro_300i_comm( SERIAL_READ, reply + already_read, &len ) )
        {
            already_read += len;
            if (    already_read >= 5
                 && ! strncmp( reply + already_read - 5, " ok\r\n", 5 ) )
                break;
        }
        stop_on_user_request( );
    }

    if ( i == 10 )
        spectrapro_300i_comm_fail( );

    spectrapro_300i_get_gratings( );
    spectrapro_300i.current_gn = spectrapro_300i_get_grating( );
    spectrapro_300i.tn = spectrapro_300i_get_turret( );

    if ( spectrapro_300i.is_exit_mirror )
        spectrapro_300i_set_exit_mirror( spectrapro_300i.exit_mirror );
}


/*--------------------------------------------------------------*
 * Function just closes the device file for the serial port the
 * monochromator is attached to.
 *--------------------------------------------------------------*/

void
spectrapro_300i_close( void )
{
    if ( spectrapro_300i.is_open )
        spectrapro_300i_comm( SERIAL_EXIT );
    spectrapro_300i.is_open = UNSET;
}


/*-------------------------------------------------------*
 * Function asks the monochromator for the wavelength it
 * is currently set to.
 *-------------------------------------------------------*/

double
spectrapro_300i_get_wavelength( void )
{
    char *reply;
    double wl;


    reply = spectrapro_300i_talk( "?NM", 100 );
    wl =  T_atod( reply ) * 1.0e-9;
    T_free( ( void * ) reply );
    return wl;
}


/*------------------------------------------------------*
 * Function sets the monochromator to a new wavelength.
 *------------------------------------------------------*/

void
spectrapro_300i_set_wavelength( double wavelength )
{
    char *buf;


    fsc2_assert( wavelength < 0.0 || wavelength <= MAX_WAVELENGTH );

    buf = get_string( "%.3f GOTO", 1.0e9 * wavelength );

    TRY
    {
        spectrapro_300i_send( buf );
        T_free( buf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( buf );
        RETHROW;
    }
}


/*----------------------------------------------------------------*
 * Function asks the monochromator for the turret currently used.
 *----------------------------------------------------------------*/

long
spectrapro_300i_get_turret( void )
{
    const char *reply;
    long tn;


    reply = spectrapro_300i_talk( "?TURRET", 20 );
    tn = T_atol( reply ) - 1;
    T_free( ( void * ) reply );
    return tn;
}


/*-------------------------------------------------------------------*
 * Function tells the monochromator to switch to a different turret.
 *-------------------------------------------------------------------*/

void
spectrapro_300i_set_turret( long tn )
{
    char *buf;


    fsc2_assert( tn >= 0 && tn < MAX_TURRETS );

    if ( spectrapro_300i.tn == tn )
        return;

    buf = get_string( "%ld TURRET", tn + 1 );

    TRY
    {
        spectrapro_300i_send( buf );
        T_free( buf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( buf );
        RETHROW;
    }
}


/*-----------------------------------------------------------------*
 * Function asks the monochromator for the currently used grating.
 *-----------------------------------------------------------------*/

long
spectrapro_300i_get_grating( void )
{
    const char *reply;
    long gn;


    reply = spectrapro_300i_talk( "?GRATING", 20 );
    gn = T_atol( reply ) - 1;
    T_free( ( void * ) reply );
    return gn;
}


/*-------------------------------------------------------------------*
 * Function tells to monochromator to switch to a different grating.
 *-------------------------------------------------------------------*/

void
spectrapro_300i_set_grating( long gn )
{
    char *buf;


    fsc2_assert(    gn >= 0 && gn < MAX_GRATINGS
                 && gn - spectrapro_300i.tn * 3 >= 0
                 && gn - spectrapro_300i.tn * 3 <= 2
                 && spectrapro_300i.grating[ gn ].is_installed );

    if ( spectrapro_300i.current_gn == gn )
        return;

    buf = get_string( "%ld GRATING", gn + 1 );

    TRY
    {
        spectrapro_300i_send( buf );
        T_free( buf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( buf );
        RETHROW;
    }
}


/*-----------------------------------------------------------------*
 * Please note: the manual claims that sending "EXIT-MIRROR" would
 * not be required but without it it doesn't work reliable!
 *-----------------------------------------------------------------*/

int
spectrapro_300i_get_exit_mirror( void )
{
    const char *reply;
    long exit_mirror;


    spectrapro_300i_send( "EXIT-MIRROR" );
    reply = spectrapro_300i_talk( "?MIR", 20 );
    exit_mirror = T_atol( reply );
    T_free( ( void * ) reply );
    return exit_mirror;
}


/*-----------------------------------------------------------------*
 * Please note: the manual claims that sending "EXIT-MIRROR" would
 * not be required but without it it doesn't work reliable!
 *-----------------------------------------------------------------*/

void
spectrapro_300i_set_exit_mirror( long exit_mirror )
{
    spectrapro_300i_send( "EXIT-MIRROR" );
    spectrapro_300i_send( exit_mirror ? "SIDE" : "FRONT" );
}


/*-----------------------------------------------------------------*
 * Function queries the monochromator about informations about all
 * installed gratings and stores the data (groove density, blaze
 * information) in the internal structure for the monochromator.
 *-----------------------------------------------------------------*/

void
spectrapro_300i_get_gratings( void )
{
    const char *reply;
    const char *sp;
    long gn, gr, bl;
    int i;


    reply = spectrapro_300i_talk( "?GRATINGS", 80 * MAX_GRATINGS );

    for ( sp = reply, i = 0; i < MAX_GRATINGS; i++ )
    {
        while ( *sp != '\0' && ! isdigit( ( unsigned char ) *sp ) )
            sp++;

        if ( *sp == '\0' )
        {
            T_free( ( void * ) reply );
            spectrapro_300i_comm_fail( );
        }

        gn = 0;
        while( *sp != '\0' && isdigit( ( unsigned char ) *sp ) )
            gn = gn * 10 + ( long ) ( *sp++ - '0' );

        if ( *sp == '\0' || gn - 1 != i )
        {
            T_free( ( void * ) reply );
            spectrapro_300i_comm_fail( );
        }

        while ( *sp != '\0' && isspace( ( unsigned char ) *sp ) )
            sp++;

        if ( *sp == '\0' )
        {
            T_free( ( void * ) reply );
            spectrapro_300i_comm_fail( );
        }

        if ( ! strncmp( sp, "Not Installed", 13 ) )
        {
            sp += 13;
            continue;
        }

        if ( ! isdigit( ( unsigned char ) *sp ) )
        {
            T_free( ( void * ) reply );
            spectrapro_300i_comm_fail( );
        }

        gr = 0;
        while( *sp != '\0' && isdigit( ( unsigned char ) *sp ) )
            gr = gr * 10 + ( long ) ( *sp++ - '0' );

        if ( *sp == '\0' )
        {
            T_free( ( void * ) reply );
            spectrapro_300i_comm_fail( );
        }

        spectrapro_300i.grating[ i ].grooves = gr * 1000;

        while ( *sp != '\0' && isspace( ( unsigned char ) *sp ) )
            sp++;

        if ( *sp == '\0' )
        {
            T_free( ( void * ) reply );
            spectrapro_300i_comm_fail( );
        }

        if ( strncmp( sp, "g/mm BLZ=", 9 ) )
        {
            T_free( ( void * ) reply );
            spectrapro_300i_comm_fail( );
        }

        sp += 9;

        while ( *sp != '\0' && isspace( ( unsigned char ) *sp ) )
            sp++;

        if ( *sp == '\0' )
        {
            T_free( ( void * ) reply );
            spectrapro_300i_comm_fail( );
        }

        if ( ! isdigit( ( unsigned char ) *sp ) )
        {
            spectrapro_300i.grating[ i ].blaze = -1;
            while ( *sp != '\0' && isalpha( ( unsigned char ) *sp ) )
                sp++;
            if ( *sp == '\0' )
            {
                T_free( ( void * ) reply );
                spectrapro_300i_comm_fail( );
            }
        }
        else
        {
            bl = 0;
            while( *sp != '\0' && isdigit( ( unsigned char ) *sp ) )
                bl = bl * 10 + ( long ) ( *sp++ - '0' );

            if ( *sp == '\0' )
            {
                T_free( ( void * ) reply );
                spectrapro_300i_comm_fail( );
            }

            spectrapro_300i.grating[ i ].blaze = bl * 1.0e-9;

            while ( *sp != '\0' && isspace( ( unsigned char ) *sp ) )
                sp++;

            if ( *sp == '\0' )
            {
                T_free( ( void * ) reply );
                spectrapro_300i_comm_fail( );
            }

            if ( *sp == '\0' )
            {
                T_free( ( void * ) reply );
                spectrapro_300i_comm_fail( );
            }

            if ( strncmp( sp, "nm", 2 ) )
            {
                T_free( ( void * ) reply );
                spectrapro_300i_comm_fail( );
            }

            sp += 2;
        }

        spectrapro_300i.grating[ i ].is_installed = SET;
    }

    T_free( ( void * ) reply );
}


/*-----------------------------------------------------*
 * Function asks the monochromator for the zero offset
 * setting for one of the gratings.
 *-----------------------------------------------------*/

long
spectrapro_300i_get_offset( long gn )
{
    const char *reply;
    char *sp, *nsp;
    long offset = 0;
    long i;


    fsc2_assert( gn >= 0 && gn < MAX_GRATINGS );

    reply = spectrapro_300i_talk( "MONO-EESTATUS", 4096 );

    if ( ( sp = strstr( reply, "offset" ) ) == NULL )
    {
        T_free( ( void * ) reply );
        spectrapro_300i_comm_fail( );
    }

    sp += strlen( "offset" );

    for ( i = 0; i <= gn; i++ )
    {
        offset = strtol( sp, &nsp, 10 );
        if (    sp == nsp
             || (    ( offset == LONG_MIN || offset == LONG_MAX )
                  && errno == ERANGE ) )
        {
            T_free( ( void * ) reply );
            spectrapro_300i_comm_fail( );
        }
        sp = nsp;
    }

    T_free( ( void * ) reply );
    return offset;
}


/*----------------------------------------------------------*
 * Function sets a new zero offset for one of the gratings,
 * then executes a reset and finally switches back to the
 * original wavelength. The function needs quite a lot of
 * time, mainly because of the required reset.
 *----------------------------------------------------------*/

void
spectrapro_300i_set_offset( long gn,
                            long offset )
{
    fsc2_assert( gn >= 0 && gn < MAX_GRATINGS );
    fsc2_assert( spectrapro_300i.grating[ gn ].is_installed );
    fsc2_assert ( labs( offset ) <= lrnd( ( double ) INIT_OFFSET_RANGE /
                                          spectrapro_300i.grating[ gn ].grooves
                                          + ( gn % 3 ) * INIT_OFFSET ) );

    char * volatile buf = get_string( "%ld INIT-GRATING", gn + 1 );

    TRY
    {
        spectrapro_300i_send( buf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( buf );
        RETHROW;
    }

    T_free( buf );
    buf = get_string( "%.3f INIT-WAVELENGTH",
                      1.0e9 * spectrapro_300i.wavelength );

    TRY
    {
        spectrapro_300i_send( buf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( buf );
        RETHROW;
    }

    T_free( buf );
    buf = get_string( "%ld %ld INIT-SP300-OFFSET", offset, gn );

    TRY
    {
        spectrapro_300i_send( buf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( buf );
        RETHROW;
    }

    T_free( buf );
    buf = spectrapro_300i_talk( "MONO-RESET", 4096 );
    T_free( buf );
}


/*--------------------------------------------------------*
 * Function asks the monochromator for the grating adjust
 * setting for one of the gratings.
 *--------------------------------------------------------*/

long
spectrapro_300i_get_adjust( long gn )
{
    const char *reply;
    char *sp, *nsp;
    long gadjust = 0;
    long i;


    fsc2_assert( gn >= 0 && gn < MAX_GRATINGS );

    reply = spectrapro_300i_talk( "MONO-EESTATUS", 4096 );

    if ( ( sp = strstr( reply, "adjust" ) ) == NULL )
    {
        T_free( ( void * ) reply );
        spectrapro_300i_comm_fail( );
    }

    sp += strlen( "adjust" );

    for ( i = 0; i <= gn; i++ )
    {
        gadjust = strtol( sp, &nsp, 10 );
        if (    sp == nsp
             || (    ( gadjust == LONG_MIN || gadjust == LONG_MAX )
                  && errno == ERANGE ) )
        {
            T_free( ( void * ) reply );
            spectrapro_300i_comm_fail( );
        }
        sp = nsp;
    }

    T_free( ( void * ) reply );
    return gadjust;
}


/*-------------------------------------------------------------------*
 * Function sets a new grating adjust value for one of the gratings,
 * then executes a reset and finally switches back to the original
 * wavelength. The function needs quite a lot of time, mainly
 * because of the required reset.
 *-------------------------------------------------------------------*/

void
spectrapro_300i_set_adjust( long gn,
                            long adjust )
{
    fsc2_assert( gn >= 0 && gn < MAX_GRATINGS );
    fsc2_assert( spectrapro_300i.grating[ gn ].is_installed );
    fsc2_assert ( labs( adjust - INIT_ADJUST ) <= INIT_ADJUST_RANGE );

    char * volatile buf = get_string( "%ld INIT-GRATING", gn + 1 );

    TRY
    {
        spectrapro_300i_send( buf );
        T_free( buf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( buf );
        RETHROW;
    }

    buf = get_string( "%.3f INIT-WAVELENGTH",
                      1.0e9 * spectrapro_300i.wavelength );

    TRY
    {
        spectrapro_300i_send( buf );
        T_free( buf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( buf );
        RETHROW;
    }

    buf = get_string( "%ld %ld INIT-SP300-GADJUST", adjust, gn );

    TRY
    {
        spectrapro_300i_send( buf );
        T_free( buf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( buf );
        RETHROW;
    }

    buf = spectrapro_300i_talk( "MONO-RESET", 4096 );
    T_free( buf );
}


/*---------------------------------------------------------------------*
 * Function installs a new grating into the non-volatile memory of the
 * monochromator. But there's a twist: there are a number of gratings
 * listed in the manual for the device and the device only accepts
 * those. For other gratings (which are officially sold as gratings
 * for the device) there seems to be a frimware bug, resulting in the
 * non-volatile memory becoming garbled. For these gratings a standard
 * grating has to be installed with the "INSTALL" command first and
 * then the blaze wavelength has to be adjusted.
 *---------------------------------------------------------------------*/

void
spectrapro_300i_install_grating( long         gn,
                                 const char * part_no )
{
    char *buf;
    size_t i;
    volatile bool is_supported = SET;
    const char *blaze_id = part_no + 6;
    char reply[ 8 ];
    char blaze[ ] = "       ";    /* seven spaces and a '\0' character */


    fsc2_assert( gn >= 0 && gn < MAX_GRATINGS );

    /* Test if this is one of the gratings listed in the manual */

    for ( i = 0; i < NUM_ELEMS( good_gratings ); i++ )
        if ( ! strcmp( good_gratings[ i ], part_no ) )
            break;

    /* If it's not pick one with the same number of grooves but a different
       blaze wavelength (if possible) */

    if ( i == NUM_ELEMS( good_gratings ) )
    {
        is_supported = UNSET;
        for ( i = 0; i < NUM_ELEMS( good_gratings ); i++ )
            if ( ! strncmp( good_gratings[ i ], part_no, 6 ) )
                break;
    }

    if ( i == NUM_ELEMS( good_gratings ) )
    {
        print( FATAL, "Grating with part no. '%s' isn't a supported grating.\n",
               part_no );
        THROW( EXCEPTION );
    }

    buf = get_string( "%s %ld INSTALL", good_gratings[ i ], gn + 1 );

    TRY
    {
        spectrapro_300i_send( buf );
        T_free( buf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( buf );
        RETHROW;
    }

    /* If this was for one of the gratings from the manual were done */

    if ( is_supported )
        return;

    /* For "non-standard gratings the blaze part is either a three-digit
       number for the blaze wavelength in nm, a one- or two-digit string
       or two digits, separated by a dot for the blaze wavelenght in um,
       or some string not all being numbers. We have to convert this to
       a string to be set as the blaze part in the grating identifier. */

    strncpy( blaze, blaze_id, strlen( blaze_id ) );

    if (    strlen( blaze_id ) == 3
         && isdigit( ( unsigned char ) blaze_id[ 0 ] )
         && isdigit( ( unsigned char ) blaze_id[ 1 ] )
         && isdigit( ( unsigned char ) blaze_id[ 2 ] ) )
        strncpy( blaze + 4, "nm", 2 );
    else if (    ( strlen( blaze_id ) == 1 &&
                   isdigit( ( int ) blaze_id[ 0 ] ) )
              || (   strlen( blaze_id ) == 2
                   && isdigit( ( unsigned char ) blaze_id[ 0 ] )
                   && isdigit( ( unsigned char ) blaze_id[ 1 ] ) )
                 || ( strlen( blaze_id ) == 3
                   && isdigit( ( unsigned char ) blaze_id[ 0 ] )
                   && blaze_id[ 1 ] == '.'
                   && isdigit( ( unsigned char ) blaze_id[ 2 ] ) ) )
        strncpy( blaze + strlen( blaze_id ) + 1, "um", 2 );

    /* Setting the blaze wavelenght is a bit of bother since the BLAZE command
       works differently from all other commands. It requires first sending
       "BLAZE\r", then reading in a string consisting of 7 chars (I don't know
       exactly what they are) and a CR ('\r'), and then sending a string
       consisting of 7 characters (and no final CR as with other commands).
       As a reply the device is supposed to send "ok\r\n" or " ok\r\n". */

    i = 8;
    if (    ! spectrapro_300i_comm( SERIAL_WRITE, "BLAZE\r" )
         || ! spectrapro_300i_comm( SERIAL_READ, reply, &i )
         || i != 8 )
        spectrapro_300i_comm_fail( );

    if ( reply[ 7 ] != '\r' )
        spectrapro_300i_comm_fail( );

    if ( ! spectrapro_300i_comm( SERIAL_WRITE, blaze ) )
        spectrapro_300i_comm_fail( );

    i = 5;
    if (    ! spectrapro_300i_comm( SERIAL_READ, reply, &i )
         || i < 4 )
        spectrapro_300i_comm_fail( );

    if ( strncmp( reply, "ok\r\n", 4 ) && strncmp( reply, " ok\r\n", 5 ) )
        spectrapro_300i_comm_fail( );
}


/*--------------------------------------------------------*
 * Function removes an already installed grating from the
 * non-volatile memory of the monochromator.
 *--------------------------------------------------------*/

void
spectrapro_300i_uninstall_grating( long gn )
{
    char *buf;


    fsc2_assert( gn >= 0 && gn < MAX_GRATINGS );

    buf = get_string( "%ld UNINSTALL", gn + 1 );

    TRY
    {
        spectrapro_300i_send( buf );
        T_free( buf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( buf );
        RETHROW;
    }
}


/*--------------------------------------------------------*
 * Function for commands that just get send to the device
 * and don't expect any replies.
 *--------------------------------------------------------*/

void
spectrapro_300i_send( const char * buf )
{
    char *lbuf;
    size_t len;
    char reply[ 5 ];


    fsc2_assert( buf != NULL && *buf != '\0' );

    lbuf = get_string( "%s\r", buf );
    len = strlen( lbuf );

    if ( ! spectrapro_300i_comm( SERIAL_WRITE, lbuf ) )
    {
        T_free( lbuf );
        spectrapro_300i_comm_fail( );
    }

    /* The device always echos the command, we got to get rid of the echo */

    TRY
    {
        spectrapro_300i_read( lbuf, &len );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( lbuf );
        RETHROW;
    }

    T_free( lbuf );

    /* Read the string returned by the device indicating success */

    len = 5;
    if ( ! spectrapro_300i_read( reply, &len ) )
        spectrapro_300i_comm_fail( );
}


/*-----------------------------------------------------------------------*
 * Function tries to read up to '*len' bytes of data into 'buf' from the
 * monochromator. It recognizes the end of the data (if there are less
 * than '*len' going to be send by the device) by the string " ok\r\n"
 * (or, in one case "ok\r\n") when the command initializing the read
 * was successful, or "?\r\n" when the command failed (probably due to
 * an invalid command.
 * There are four cases to be considered:
 * 1. The returned string ended in "ok\r\n" or " ok\r\n". In this case
 *    this part is cut of (i.e. replaced by a '\0') and the length of
 *    the string ready in is returned in len (could actually be 0 when
 *    the "ok\r\n" part was everything we got) and the function returns
 *    a status indicating success.
 * 2. The returned string ended ended neither in "ok\r\n", " ok\r\n" or
 *    "?\r\n", indicating that there are more data coming. In 'len' the
 *    length of what we got is returned and the function returns a
 *    status indicating failure. No '\0' is appended to the returned
 *    string.
 * 3. The string we got ended in "?\r\n", in which case the function
 *    throws an exception.
 * 4. Reading from the device failed, in which case an exception is
 *    thrown.
 * Some care has to be taken: when the input buffer 'buf' isn't large
 * enough to hold the complete string the device is trying to send it
 * may happen that the transmission ends within the marker indicating
 * success or failure, in which case this function won't be able to
 * determine if the end of a transmission has been reached. In this
 * case the calling function must do the checking!
 *
 * There's also another way this function can be ended: if the user hit
 * the "Stop" button a USER_BREAK_EXCEPTION is thrown.
 *-----------------------------------------------------------------------*/

static
bool spectrapro_300i_read( char *   buf,
                           size_t * len )
{
    size_t volatile to_fetch = *len;
    size_t volatile already_read = 0;
    char *lbuf;
    long llen = *len;
    bool volatile done = UNSET;


    lbuf = T_malloc( llen );

    do
    {
        llen = to_fetch;
        if ( ! spectrapro_300i_comm( SERIAL_READ, buf + already_read, &llen ) )
        {
            T_free( lbuf );
            spectrapro_300i_comm_fail( );
        }

        /* Device didn't send anything yet then try again. */

        if ( llen == 0 )
            goto read_retry;

        already_read += llen;
        to_fetch -= llen;

        /* No end marker can have been read yet */

        if ( already_read < 3 )
        {
            if ( to_fetch == 0 )
                break;
            goto read_retry;
        }

        /* Throw exception if device did signal an invalid command */

        if ( ! strncmp( buf + already_read - 3, "?\r\n", 3 ) )
        {
            T_free( lbuf );
            spectrapro_300i_comm_fail( );
        }

        /* No end marker indicating success can have been read yet */

        if ( already_read < 4 )
        {
            if ( to_fetch == 0 )
                break;
            goto read_retry;
        }

        /* Check if we got an indicator saying that everything the device is
           going to write has already been sent successfully - if yes replace
           the indicator by a '\0' character and break from the loop */

        if ( ! strncmp( buf + already_read - 4, "ok\r\n", 4 ) )
        {
            already_read -= 4;
            if ( already_read > 0 && buf[ already_read - 1 ] == ' ' )
                already_read--;
            buf[ already_read ] = '\0';
            done = SET;
            break;
        }

        /* When we get here we have to try again reading (more) data. Before
           we do we wait a bit and also check if the "Stop" button has been
           hit by the user. */

    read_retry:

        TRY
        {
            stop_on_user_request( );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( lbuf );
            RETHROW;
        }
    } while ( to_fetch > 0 );

    *len = already_read;
    return done;
}


/*---------------------------------------------------------------*
 * Function sends a command and returns a buffer (with a default
 * size of *len bytes) with the reply of the device. If the
 * reply by the device is longer that *len bytes, a larger
 * buffer gets returned. The buffer always ends in a '\0'.
 *---------------------------------------------------------------*/

char *
spectrapro_300i_talk( const char * buf,
                      size_t       len )
{
    fsc2_assert( buf != NULL && *buf != '\0' && len != 0 );

    char * volatile lbuf = get_string( "%s\r", buf );
    size_t comm_len = strlen( lbuf );

    if ( ! spectrapro_300i_comm( SERIAL_WRITE, lbuf ) )
    {
        T_free( lbuf );
        spectrapro_300i_comm_fail( );
    }

    /* The device always echos the command, we got to get rid of this echo. */

    TRY
    {
        spectrapro_300i_read( lbuf, &comm_len );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( lbuf );
        RETHROW;
    }

    /* Now we read the reply by the device, if necessary extending the
       buffer. */

    size_t volatile already_read = 0;

    TRY
    {
        len += 5;
        lbuf = T_realloc( lbuf, len );

        while ( ! spectrapro_300i_read( lbuf + already_read, &len ) )
        {
            lbuf = T_realloc( lbuf, 2 * len + 5 );
            already_read += len;
        }
        already_read += len;
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( lbuf );
        RETHROW;
    }

    T_realloc( lbuf, already_read + 1 );
    return lbuf;
}


/*-----------------------------------------------------------------*
 * Low level function for the communication with the monochromator
 * via the serial port.
 *-----------------------------------------------------------------*/

static bool
spectrapro_300i_comm( int type,
                      ... )
{
    va_list ap;
    char *buf;
    ssize_t len;
    size_t *lptr;


    switch ( type )
    {
        case SERIAL_INIT :
            /* Open the serial port for reading and writing. */

            if ( ( spectrapro_300i.tio = fsc2_serial_open( spectrapro_300i.sn,
                                                           O_RDWR ) ) == NULL )
                return FAIL;

            /* Set transfer mode to 8 bit, no parity and 1 stop bit (8-N-1)
               and ignore control lines, don't use flow control. */

            spectrapro_300i.tio->c_cflag = 0;
            spectrapro_300i.tio->c_cflag = CLOCAL | CREAD | CS8;
            spectrapro_300i.tio->c_iflag = IGNBRK;
            spectrapro_300i.tio->c_oflag = 0;
            spectrapro_300i.tio->c_lflag = 0;
            cfsetispeed( spectrapro_300i.tio, SERIAL_BAUDRATE );
            cfsetospeed( spectrapro_300i.tio, SERIAL_BAUDRATE );

            fsc2_tcflush( spectrapro_300i.sn, TCIFLUSH );
            fsc2_tcsetattr( spectrapro_300i.sn, TCSANOW, spectrapro_300i.tio );
            break;

        case SERIAL_EXIT :                    /* reset and close serial port */
            fsc2_serial_close( spectrapro_300i.sn );
            break;

        case SERIAL_WRITE :
            va_start( ap, type );
            buf = va_arg( ap, char * );
            va_end( ap );

            len = strlen( buf );
            if ( fsc2_serial_write( spectrapro_300i.sn, buf, len,
                                    0, UNSET ) != len )
                return FAIL;
            break;

        case SERIAL_READ :
            va_start( ap, type );
            buf = va_arg( ap, char * );
            lptr = va_arg( ap, size_t * );
            va_end( ap );

            len = fsc2_serial_read( spectrapro_300i.sn, buf, *lptr, NULL,
                                    SPECTRAPRO_300I_WAIT, UNSET );
            if ( len < 0 )
                return FAIL;
            else
                *lptr = len;
            break;

        default :
            print( FATAL, "INTERNAL ERROR detected at %s:%d.\n",
                   __FILE__, __LINE__ );
            THROW( EXCEPTION );
    }

    return OK;
}


/*-----------------------------------------*
 * Converts a wavelength into a wavenumber
 *-----------------------------------------*/

double
spectrapro_300i_wl2wn( double wl )
{
    fsc2_assert( wl > 0 );
    return 0.01 / wl;
}


/*-----------------------------------------*
 * Converts a wavenumber into a wavelength
 *-----------------------------------------*/

double
spectrapro_300i_wn2wl( double wn )
{
    fsc2_assert( wn > 0 );
    return 0.01 / wn;
}


/*----------------------------------------------------------*
 * Function called on communication failure with the device
 *----------------------------------------------------------*/

static
void spectrapro_300i_comm_fail( void )
{
    print( FATAL, "Can't access the monochromator.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
