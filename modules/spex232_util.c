/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
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


#include "spex232.h"


/*---------------------------------------------------------------------*
 * Function for reading in the last stored state of the monochromator.
 * The state consists of the calibration offset value, the wavelength
 * or wavenumber (depending on the type of the monochromator) spacing
 * between pixels for the connected CCD camera and, for wavenumber
 * driven monochromators, the position of the laser line.
 *---------------------------------------------------------------------*/

bool
spex232_read_state( void )
{
    char *fn;
    const char *dn;
    FILE *fp;
    int c;
    int ret;
    double val[ 4 ];
    double new;
    int i = 0;
    bool in_comment = UNSET;
    bool found_end = UNSET;


    dn = fsc2_config_dir( );
    fn = get_string( "%s%s%s", dn, slash( dn ), SPEX232_STATE_FILE );

    if ( ( fp = fsc2_fopen( fn, "r" ) ) == NULL )
    {
        print( FATAL, "Can't open state file '%s'.\n", fn );
        T_free( fn );
        SPEX232_THROW( EXCEPTION );
    }

    do
    {
        c = fsc2_fgetc( fp );

        switch ( c )
        {
            case EOF :
                found_end = SET;
                break;

            case '#' :
                in_comment = SET;
                break;

            case '\n' :
                in_comment = UNSET;
                break;

            default :
                if ( in_comment || isspace( c ) )
                    break;

                fsc2_fseek( fp, -1, SEEK_CUR );

                ret = fsc2_fscanf( fp, "%lf", &new );

                if (    ( ret == 1
                          && (    ( spex232.mode & WL && i >= 3 )
                                  || ( spex232.mode & WN_MODES && i >= 4 ) ) )
                     || ret != 1 )
                {
                    print( FATAL, "Invalid state file '%s'.\n", fn );
                    T_free( fn );
                    fsc2_fclose( fp );
                    SPEX232_THROW( EXCEPTION );
                }

                val[ i++ ] = new;

                while ( ( c = fsc2_fgetc( fp ) ) != EOF && isspace( c ) )
                    /* empty */ ;

                if (    c == EOF
                     || fgetc( fp ) != 'm'
                     || (    spex232.mode & WN_MODES && i == 4 && c != 'c'
                          && fgetc( fp ) != '^' && fgetc( fp ) != '-'
                          && fgetc( fp ) != '1' )
                     || ( i < 4 && c != 'n' ) )
                {
                    print( FATAL, "Invalid state file '%s'.\n", fn );
                    T_free( fn );
                    fsc2_fclose( fp );
                    SPEX232_THROW( EXCEPTION );
                }

                in_comment = SET;
        }
    } while ( ! found_end );

    fsc2_fclose( fp );

    if ( val[ 0 ] < 0.0 || val[ 2 ] < 0.0 )
    {
        print( FATAL, "Invalid state file '%s'.\n", fn );
        T_free( fn );
        SPEX232_THROW( EXCEPTION );
    }

    spex232.motor_position = spex232_wl2p( 1.0e-9 * val[ 0 ] );
    spex232.offset         = 1.0e-9 * val[ 1 ];
    spex232.pixel_diff     = 1.0e-9 * val[ 2 ];

    if ( spex232.mode & WN_MODES )
    {
        if ( val[ 3 ] < 0.0 )
        {
            print( FATAL, "Invalid state file '%s'.\n", fn );
            T_free( fn );
            SPEX232_THROW( EXCEPTION );
        }

        spex232.laser_line = val[ 3 ];
    }

    T_free( fn );
    return OK;
}


/*---------------------------------------------------------------*
 * Function for writing the state of the monochromator to a file
 *---------------------------------------------------------------*/

bool
spex232_store_state( void )
{
    char *fn;
    const char *dn;
    FILE *fp;


    dn = fsc2_config_dir( );
    fn = get_string( "%s%s%s", dn, slash( dn ), SPEX232_STATE_FILE );

    if ( ( fp = fsc2_fopen( fn, "w" ) ) == NULL )
    {
        print( SEVERE, "Can't store state data in '%s', can't open file "
               "for writing.\n", fn );
        T_free( fn );
        return FAIL;
    }

    T_free( fn );

    fsc2_fprintf( fp, "# --- Do *not* edit this file, it gets created "
                  "automatically ---\n\n"
                  "# In this file state information for the spex232 "
                  "driver is stored:\n"
                  "# 1. The wavelength the monochromator is set to\n"
                  "# 2. An offset of the monochromator, i.e. a number that "
                  "always has to be\n"
                  "#    added to wavelengths specified by the user\n"
                  "# 3. The wavelength difference between two pixels of the "
                  "connected CCD camera\n"
                  "# 4. For wavenumber driven monochromators only: the "
                  "current setting of\n"
                  "#    the position of the laser line\n\n" );

    if ( spex232.mode & WN_MODES )
        fsc2_fprintf( fp, "%.5f nm\n%.5f nm\n%.7f nm\n%.4f cm^-1\n",
                      1.0e9 * spex232_p2wl( spex232.motor_position ),
                      1.0e9 * spex232.offset,
                      1.0e9 * spex232.pixel_diff,
                      spex232.laser_line );
    else
        fsc2_fprintf( fp, "%.5f nm\n%.5f nm\n%.7f nm\n",
                      1.0e9 * spex232_p2wl( spex232.motor_position ),
                      1.0e9 * spex232.offset,
                      1.0e9 * spex232.pixel_diff );

    fsc2_fclose( fp );

    return OK;
}


/*-------------------------------------------------------------*
 * All the following functions are for conversions. We often
 * have to convert between wavenumbers and wavelengths as seen
 * by the user (i.e. including corrections and given in either
 * absolute or relative wavenumbers, depending on the current
 * mode) and the units they are stored here (in the units to
 * be send to the monochromator) and vice versa.
 *-------------------------------------------------------------*/


/*---------------------------------------------------*
 * Converts a wavelength into an absolute wavenumber
 *---------------------------------------------------*/

double
spex232_wl2wn( double wl )
{
    if ( wl <= 0.0 )
        SPEX232_THROW( INVALID_INPUT_EXCEPTION );
    return 0.01 / wl;
}


/*---------------------------------------------------*
 * Converts an absolute wavenumber into a wavelength
 *---------------------------------------------------*/

double
spex232_wn2wl( double wn )
{
    if ( wn <= 0.0 )
        SPEX232_THROW( INVALID_INPUT_EXCEPTION );
    return 0.01 / wn;
}


/*-----------------------------------------------------------*
 * Converts internally used wavelength to the value expected
 * by the user (i.e. with correction included)
 *-----------------------------------------------------------*/

double
spex232_wl2Uwl( double wl )
{
    wl -= spex232.offset;
    if ( wl <= 0.0 )
        SPEX232_THROW( INVALID_INPUT_EXCEPTION );
    return wl;
}


/*------------------------------------------------------*
 * Converts a wavelength as seen by the user (i.e. with
 * correction included) to the internally used value.
 *------------------------------------------------------*/

double
spex232_Uwl2wl( double wl )
{
    wl += spex232.offset;
    if ( wl <= 0.0 )
        SPEX232_THROW( INVALID_INPUT_EXCEPTION );
    return wl;
}


/*-----------------------------------------------------------*
 * Converts internally used wavenumber to the value expected
 * by the user (i.e. including the correction and converting
 * it to relative units if a laser line is set).
 *-----------------------------------------------------------*/

double
spex232_wn2Uwn( double wn )
{
    wn = spex232_wl2wn( spex232_wl2Uwl( spex232_wn2wl( wn ) ) );

    if ( spex232.mode == WN_REL )
        wn = spex232.laser_line - wn;

    return wn;
}


/*-----------------------------------------------------------*
 * Converts a wavenumber from the way the user sees it (i.e.
 * including the correction and in relative units if a laser
 * line is set) to the internally used value.
 *-----------------------------------------------------------*/

double
spex232_Uwn2wn( double wn )
{
    if ( spex232.mode == WN_REL )
        wn = spex232.laser_line - wn;

    return spex232_wl2wn( spex232_Uwl2wl( spex232_wn2wl( wn ) ) );
}


/*-----------------------------------------------------------*
 * Converts from internally used wavelength to a wavenumber
 * as expected by the user (i.e. including offset correction
 * and relative to the laser line if one is set).
 *-----------------------------------------------------------*/

double
spex232_wl2Uwn( double wl )
{
    double wn = spex232_wl2wn( spex232_wl2Uwl( wl ) );

    if ( spex232.mode == WN_REL )
        wn = spex232.laser_line - wn;

    return wn;
}


/*----------------------------------------------------------------*
 * Converts from a wavelength as seen by the user (i.e. including
 * offset correction) to internally used wavenumber.
 *----------------------------------------------------------------*/

double
spex232_Uwl2wn( double wl )
{
    return spex232_wl2wn( spex232_Uwl2wl( wl ) );
}


/*--------------------------------------------------------*
 * Converts internally used wavenumber to a wavelength as
 * seen by the user (i.e. including offset correction).
 *--------------------------------------------------------*/

double
spex232_wn2Uwl( double wn )
{
    return spex232_wl2Uwl( spex232_wn2wl( wn ) );
}


/*-----------------------------------------------------------*
 * Converts a wavenumber as seen by the user (i.e. including
 * offset correction and in relative wavenumber mode if
 * necessary) to internally used wavelength.
 *-----------------------------------------------------------*/

double
spex232_Uwn2wl( double wn )
{
    return spex232_wn2wl( spex232_Uwn2wn( wn ) );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
