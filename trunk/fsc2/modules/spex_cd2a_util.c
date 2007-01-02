/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2007 Jens Thoms Toerring
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


#include "spex_cd2a.h"


/*---------------------------------------------------------------------*
 * Function for reading in the last stored state of the monochromator.
 * The state consists of the calibration offset value, the wavelength
 * or wavenumber (depending on the type of the monochromator) spacing
 * between pixels for the connected CCD camera and, for wavenumber
 * driven monochromators, the position of the laser line.
 *---------------------------------------------------------------------*/

bool spex_cd2a_read_state( void )
{
    char *fn;
    const char *dn;
    FILE *fp;
    int c;
    double val[ 4 ];
    int new;
    int i = 0;
    bool in_comment = UNSET;
    bool found_end = UNSET;


    dn = fsc2_config_dir( );
    fn = get_string( "%s%s%s", dn, slash( dn ), SPEX_CD2A_STATE_FILE );

    if ( ( fp = fsc2_fopen( fn, "r" ) ) == NULL )
    {
        print( FATAL, "Can't open state file '%s'.\n", fn );
        T_free( fn );
        SPEX_CD2A_THROW( EXCEPTION );
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
                if ( in_comment )
                    break;

                fsc2_fseek( fp, -1, SEEK_CUR );

                if ( ( spex_cd2a.mode & WL && i > 3 ) || i > 4 ||
                     ( ( ( spex_cd2a.mode & WL && i < 3 ) ||
                         ( spex_cd2a.mode & WN_MODES && i < 4 ) )&& 
                       fsc2_fscanf( fp, "%lf", val + i ) != 1 ) ||
                     ( ( ( spex_cd2a.mode & WL && i == 3 ) ||
                         ( spex_cd2a.mode & WN_MODES && i == 4 ) ) &&
                       fsc2_fscanf( fp, "%d", &new ) != 1 ) )
                {
                    print( FATAL, "Invalid state file '%s'.\n", fn );
                    T_free( fn );
                    fsc2_fclose( fp );
                    SPEX_CD2A_THROW( EXCEPTION );
                }

                while ( ( c = fsc2_fgetc( fp ) ) != EOF && isspace( c ) )
                    /* empty */ ;

                if ( ( c == EOF &&
                       ( ( i != 3 && spex_cd2a.mode & WL ) ||
                         ( i != 4 && spex_cd2a.mode & WN_MODES ) ) ) ||
                     ( ( i == 0 || i == 2 || spex_cd2a.mode & WL ) &&
                       c != 'n' ) ||
                     ( ( i == 1 || i == 3 ) && spex_cd2a.mode & WN_MODES &&
                       c != 'c' ) )
                {
                    print( FATAL, "Invalid state file '%s'.\n", fn );
                    T_free( fn );
                    fsc2_fclose( fp );
                    SPEX_CD2A_THROW( EXCEPTION );
                }
                
                if ( ( ( spex_cd2a.mode & WL && i != 3 ) || i != 4 ) &&
                     ( fsc2_fgetc( fp ) != 'm' ||
                       ( c == 'c' &&
                         ( fsc2_fgetc( fp ) != '^' ||
                           fsc2_fgetc( fp ) != '-' ||
                           fsc2_fgetc( fp ) != '1' ) ) ) )
                {
                    print( FATAL, "Invalid state file '%s'.\n", fn );
                    T_free( fn );
                    fsc2_fclose( fp );
                    SPEX_CD2A_THROW( EXCEPTION );
                }

                i++;
                break;
        }

    } while ( ! found_end );

    fsc2_fclose( fp );

    if ( val[ 0 ] < 0.0 || val[ 2 ] < 0.0 )
    {
        print( FATAL, "Invalid state file '%s'.\n", fn );
        T_free( fn );
        SPEX_CD2A_THROW( EXCEPTION );
    }   

    spex_cd2a.wavelength = 1.0e-9 * val[ 0 ];
    spex_cd2a.pixel_diff = 1.0e-9 * val[ 2 ];

    if ( spex_cd2a.mode & WN_MODES )
    {
        spex_cd2a.offset = val[ 1 ];

        if ( val[ 3 ] < 0.0 )
        {
            print( FATAL, "Invalid state file '%s'.\n", fn );
            T_free( fn );
            SPEX_CD2A_THROW( EXCEPTION );
        }

        spex_cd2a.laser_line = val[ 3 ];
    }
    else
        spex_cd2a.offset = 1.0e-9 * val[ 1 ];

    T_free( fn );
    return OK;
}


/*---------------------------------------------------------------*
 * Function for writing the state of teh monochromator to a file
 *---------------------------------------------------------------*/

bool spex_cd2a_store_state( void )
{
    char *fn;
    const char *dn;
    FILE *fp;


    dn = fsc2_config_dir( );
    fn = get_string( "%s%s%s", dn, slash( dn ), SPEX_CD2A_STATE_FILE );

    if ( ( fp = fsc2_fopen( fn, "w" ) ) == NULL )
    {
        print( SEVERE, "Can't store state data in '%s', can't open file "
               "for writing.\n", fn );
        return FAIL;
    }

    fsc2_fprintf( fp, "# --- Do *not* edit this file, it gets created "
                  "automatically ---\n\n"
                  "# In this file state information for the spex_cd2a "
                  "driver is stored:\n"
                  "# 1. The wavelength the monochromator is set to\n"
                  "# 2. An offset of the monochromator, i.e. a number that "
                  "always has to be\n"
                  "#    added to wavenumbers or -lengths specified by the "
                  "user\n"
                  "# 3. The wavelength difference between two pixels of the "
                  "connected CCD camera\n"
                  "# 4. For wavenumber driven monochromators only: the "
                  "current setting of\n"
                  "#    the position of the laser line (as set at the "
                  "SPEX CD2A)\n"
                  "# 5. a flag that indicates if a new calibration has "
                  "been done.\n\n" );

    if ( spex_cd2a.mode & WN_MODES )
        fsc2_fprintf( fp, "%.5f nm\n%.4f cm^-1\n%.7f nm\n%.4f cm^-1\n",
                      1.0e9 * spex_cd2a.wavelength,
                      spex_cd2a.offset,
                      1.0e9 * spex_cd2a.pixel_diff,
                      spex_cd2a.laser_line );
    else
        fsc2_fprintf( fp, "%.5f nm\n%.5f nm\n%.7f nm\n",
                      1.0e9 * spex_cd2a.wavelength,
                      1.0e9 * spex_cd2a.offset,
                      1.0e9 * spex_cd2a.pixel_diff );

    fprintf( fp, "%d\n", spex_cd2a.new_calibration ? 1 : 0 );

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

double spex_cd2a_wl2wn( double wl )
{
    if ( wl <= 0.0 )
        SPEX_CD2A_THROW( INVALID_INPUT_EXCEPTION );
    return 0.01 / wl;
}


/*---------------------------------------------------*
 * Converts an absolute wavenumber into a wavelength
 *---------------------------------------------------*/

double spex_cd2a_wn2wl( double wn )
{
    if ( wn <= 0.0 )
        SPEX_CD2A_THROW( INVALID_INPUT_EXCEPTION );
    return 0.01 / wn;
}


/*-----------------------------------------------------------*
 * Converts internally used wavelength to the value expected
 * my the user (i.e. with correction included)
 *-----------------------------------------------------------*/

double spex_cd2a_wl2Uwl( double wl )
{
    if ( spex_cd2a.mode & WL )
        wl -= spex_cd2a.offset;
    else
        wl = spex_cd2a_wn2wl( spex_cd2a_wl2wn( wl ) - spex_cd2a.offset );

    return wl;
}


/*------------------------------------------------------*
 * Converts a wavelength as seen by the user (i.e. with
 * correction included) to the internally used value.
 *------------------------------------------------------*/

double spex_cd2a_Uwl2wl( double wl )
{
    if ( spex_cd2a.mode & WL )
        wl += spex_cd2a.offset;
    else
        wl = spex_cd2a_wn2wl( spex_cd2a_wl2wn( wl ) + spex_cd2a.offset );

    return wl;
}


/*-----------------------------------------------------------*
 * Converts internally used wavenumber to the value expected
 * by the user (i.e. inlcuding the correction and converting
 * it to relative units if a laser line is set).
 *-----------------------------------------------------------*/

double spex_cd2a_wn2Uwn( double wn )
{
    switch ( spex_cd2a.mode )
    {
        case WND :
            wn = spex_cd2a.laser_line - wn;
            /* fall through */

        case WN :
            wn -= spex_cd2a.offset;
            break;

        case WL :
            wn = spex_cd2a_wl2wn( spex_cd2a_wl2Uwl( spex_cd2a_wn2wl( wn ) ) );
            break;

        default:
            fsc2_impossible( );
    }

    return wn;
}


/*-----------------------------------------------------------*
 * Converts a wavenumber from the way the user sees it (i.e.
 * including the correction and in relative units if a laser
 * line is set) to the internally used value.
 *-----------------------------------------------------------*/

double spex_cd2a_Uwn2wn( double wn )
{
    switch ( spex_cd2a.mode )
    {
        case WND :
            wn = spex_cd2a.laser_line - wn;
            /* fall through */

        case WN :
            wn += spex_cd2a.offset;
            break;

        case WL :
            wn = spex_cd2a_wl2wn( spex_cd2a_Uwl2wl( spex_cd2a_wn2wl( wn ) ) );
            break;
            
        default:
            fsc2_impossible( );
    }

    return wn;
}


/*--------------------------------------------------------*
 * Converts internally used wavenumber to a wavelength as
 * seen by the user (i.e. including offset correction).
 *--------------------------------------------------------*/

double spex_cd2a_wn2Uwl( double wn )
{
    if ( spex_cd2a.mode & WN_MODES )
        return spex_cd2a_wn2wl( wn - spex_cd2a.offset );

    return spex_cd2a_wl2Uwl( spex_cd2a_wn2wl( wn ) );
}


/*----------------------------------------------------------------*
 * Converts from a wavelength as seen by the user (i.e. including 
 * offset correction) to internally used wavenumber.
 *----------------------------------------------------------------*/

double spex_cd2a_Uwl2wn( double wl )
{
    if ( spex_cd2a.mode & WN_MODES )
        return spex_cd2a_wl2wn( wl ) + spex_cd2a.offset;

    return spex_cd2a_wl2wn( spex_cd2a_wl2Uwl( wl ) );
}


/*-----------------------------------------------------------*
 * Converts from internally used wavelength to wavenumber as
 * seen by the user (i.e. including offset correction and
 * in relative wavenumber mode if necessary).
 *-----------------------------------------------------------*/

double spex_cd2a_wl2Uwn( double wl )
{
    if ( spex_cd2a.mode & WN_MODES )
        return spex_cd2a_wn2Uwn( spex_cd2a_wl2wn( wl ) );

    return spex_cd2a_wl2wn( spex_cd2a_wl2Uwl( wl ) );
}


/*-----------------------------------------------------------*
 * Converts a wavenumber as seen by the user (i.e. including
 * offset correction and in relative wavenumber mode if
 * necessary) to internally used wavelength.
 *-----------------------------------------------------------*/

double spex_cd2a_Uwn2wl( double wn )
{
    if ( spex_cd2a.mode & WN_MODES )
        return spex_cd2a_wn2wl( spex_cd2a_Uwn2wn( wn ) );

    return spex_cd2a_Uwl2wl( spex_cd2a_wn2wl( wn ) );
}


/*-----------------------------------------------------------*
 * Converts from an internally used wavelength to a value as
 * to be send to the monochromator under its current setting.
 *-----------------------------------------------------------*/

double spex_cd2a_wl2mu( double wl )
{
    switch ( spex_cd2a.mode )
    {
        case WN :
            return spex_cd2a_wl2wn( wl );

        case WND :
            return spex_cd2a.laser_line - spex_cd2a_wl2wn( wl );

        case WL :
            return ( UNITS == NANOMETER ? 1.0e9 : 1.0e10 ) * wl;
    }

    fsc2_impossible( );            /* we'll never get here */

    return 0.0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
