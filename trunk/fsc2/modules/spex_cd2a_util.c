/*
  $Id$

  Copyright (C) 1999-2004 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "spex_cd2a.h"


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

bool spex_cd2a_read_state( void )
{
	char *fn;
	const char *dn;
	FILE *fp;
	int c;
	double val[ 3 ];
	int i = 0;
	bool in_comment = UNSET;
	bool found_end = UNSET;


	dn = fsc2_config_dir( );
	fn = get_string( "%s%s%s", dn, slash( dn ), SPEX_CD2A_STATE_FILE );

	if ( ( fp = fsc2_fopen( fn, "r" ) ) == NULL )
	{
		print( FATAL, "Can't open state file '%s'.\n", fn );
		T_free( fn );
		THROW( EXCEPTION );
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

				if ( ( i > 1 && spex_cd2a.mode & WL ) || i > 2 ||
					 fsc2_fscanf( fp, "%lf", val + i++ ) != 1 )
				{
					print( FATAL, "Invalid state file '%s'.\n", fn );
					T_free( fn );
					fsc2_fclose( fp );
					THROW( EXCEPTION );
				}

				while ( ( c = fsc2_fgetc( fp ) ) != EOF && isspace( c ) )
					/* empty */ ;

				if ( c == EOF || ( spex_cd2a.mode & WN_MODES && c != 'c' ) ||
					 ( spex_cd2a.mode & WL && c != 'n' ) )
				{
					print( FATAL, "Invalid state file '%s'.\n", fn );
					T_free( fn );
					fsc2_fclose( fp );
					THROW( EXCEPTION );
				}
				
				if ( fsc2_fgetc( fp ) != 'm' ||
					 ( spex_cd2a.mode & WN_MODES && 
					   ( fsc2_fgetc( fp ) != '^' ||
						 fsc2_fgetc( fp ) != '-' ||
						 fsc2_fgetc( fp ) != '1' ) ) )
				{
					print( FATAL, "Invalid state file '%s'.\n", fn );
					T_free( fn );
					fsc2_fclose( fp );
					THROW( EXCEPTION );
				}

				break;
		}

	} while ( ! found_end );

	fsc2_fclose( fp );

	if ( val[ 1 ] <= 0.0 )
	{
		print( FATAL, "Invalid state file '%s'.\n", fn );
		T_free( fn );
		THROW( EXCEPTION );
	}	

	if ( spex_cd2a.mode & WN_MODES )
	{
		spex_cd2a.offset = val[ 0 ];

		spex_cd2a.pixel_diff = val[ 1 ];

		if ( val[ 2 ] <= 0.0 )
		{
			print( FATAL, "Invalid state file '%s'.\n", fn );
			T_free( fn );
			THROW( EXCEPTION );
		}

		spex_cd2a.laser_wavenumber = val[ 2 ];
	}
	else
	{
		spex_cd2a.offset     = 1.0e-9 * val[ 0 ];
		spex_cd2a.pixel_diff = 1.0e-9 * val[ 1 ];
	}

	T_free( fn );
	return OK;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

bool spex_cd2a_store_state( void )
{
	char *fn;
	const char *dn;
	FILE *fp;


	dn = fsc2_config_dir( );
	fn = get_string( "%s%s%s", dn, slash( dn ), SPEX_CD2A_STATE_FILE );

	if ( ( fp = fsc2_fopen( fn, "w" ) ) == NULL )
	{
		print( SEVERE, "Can't store state data in '%s'.\n", fn );
		return FAIL;
	}

	fsc2_fprintf( fp, "# --- Do *not* edit this file, it gets created "
				  "automatically ---\n\n"
				  "# In this file state information for the spex_cd2a "
				  "driver is stored:\n"
				  "# 1. An offset of the monochromator, i.e. a number that "
				  "always has to be\n"
				  "#    added to wavenumbers or -lengths reported by the "
				  "monochromator and\n"
				  "#    subtracted from wavenumbers or -lengths send to the "
				  "device.\n"
				  "# 2. The wavelength or wavenumber difference between two "
				  "pixels of the\n#    connected CCD camera\n"
				  "# 3. For wavenumber driven monochromators only: the "
				  "current setting of\n"
				  "#    the position of the laser line (as set at the "
				  "SPEX CD2A).\n\n" );

	if ( spex_cd2a.mode & WN_MODES )
		fsc2_fprintf( fp, "%.4f cm^-1\n%.8f cm^-1\n%.4f cm^-1\n",
					  spex_cd2a.offset,
					  spex_cd2a.pixel_diff,
					  spex_cd2a.laser_wavenumber );
	else
		fsc2_fprintf( fp, "%.5f nm\n%.9f nm\n", 1.0e9 * spex_cd2a.offset,
					  1.0e9 * spex_cd2a.pixel_diff );

	fsc2_fclose( fp );

	return OK;
}


/*---------------------------------------------------*/
/* Converts a wavelength into an absolute wavenumber */
/*---------------------------------------------------*/

double spex_cd2a_wl2Awn( double wl )
{
	fsc2_assert( wl > 0 );
	return 0.01 / wl;
}


/*---------------------------------------------------*/
/* Converts an absolute wavenumber into a wavelength */
/*---------------------------------------------------*/

double spex_cd2a_Awn2wl( double wn )
{
	fsc2_assert( wn > 0 );
	return 0.01 / wn;
}


/*-----------------------------------------------------------*/
/* Converts from absolute wavenumber to absolute or relative */
/* wavenumber (depending on the laser line being set).       */
/*-----------------------------------------------------------*/

double spex_cd2a_Awn2Mwn( double wn )
{
	if ( spex_cd2a.mode == WND )
		wn = spex_cd2a.laser_wavenumber - wn;
	return wn;
}


/*------------------------------------------------------------*/
/* Converts absolute or relative wavenumber (depending on the */
/* laser line being set) to absolute wavenumber.              */
/*------------------------------------------------------------*/

double spex_cd2a_Mwn2Awn( double wn )
{
	if ( spex_cd2a.mode == WND )
		wn = spex_cd2a.laser_wavenumber - wn;
	return wn;
}


/*-----------------------------------------------------------*/
/* Converts a wavelength into either an absolute or relative */
/* wavenumber, depending on a laser line having been set.    */
/*-----------------------------------------------------------*/

double spex_cd2a_wl2Mwn( double wl )
{
	double wn;


	fsc2_assert( wl > 0 );
	wn = 0.01 / wl;
	return spex_cd2a_Awn2Mwn( wn );
}


/*--------------------------------------------------------------------*/
/* Converts from either an absolute or relative wavenumber, depending */
/* on a laser line having been set, to a wavelength.                  */
/*--------------------------------------------------------------------*/

double spex_cd2a_Mwn2wl( double wn )
{
	wn = spex_cd2a_Mwn2Awn( wn );
	fsc2_assert( wn > 0 );
	return 0.01 / wn;
}


/*---------------------------------------------------------*/
/* Converts from a wavelength as seen by the monochromator */
/* to a wavelength as seen by the user                     */
/*---------------------------------------------------------*/

double spex_cd2a_Swl2Uwl( double wl )
{
	if ( spex_cd2a.mode & WN_MODES )
		wl = spex_cd2a_Awn2wl( spex_cd2a_wl2Awn( wl ) - spex_cd2a.offset );
	else
		wl -= spex_cd2a.offset;
	return wl;
}


/*------------------------------------------------*/
/* Converts from a wavelength as seen by the user */
/* to a wavelength as seen by the monochromator   */
/*------------------------------------------------*/

double spex_cd2a_Uwl2Swl( double wl )
{
	if ( spex_cd2a.mode & WN_MODES )
		wl = spex_cd2a_Awn2wl( spex_cd2a_wl2Awn( wl ) + spex_cd2a.offset );
	else
		wl += spex_cd2a.offset;
	return wl;
}


/*--------------------------------------------------------------*/
/* Converts an absolute wavenumber as seen by the monochromator */
/* to an absolute wavenumber as seen by the user                */
/*--------------------------------------------------------------*/

double spex_cd2a_SAwn2UAwn( double wn )
{
	if ( spex_cd2a.mode & WN_MODES )
		wn += spex_cd2a.offset;
	else
		wn = spex_cd2a_wl2Awn( spex_cd2a_Awn2wl( wn ) + spex_cd2a.offset );
	return wn;
}


/*--------------------------------------------------------*/
/* Converts an absolute wavenumber as seen by the user to */
/* an absolute wavenumber as seen by the monochromator    */
/*--------------------------------------------------------*/

double spex_cd2a_UAwn2SAwn( double wn )
{
	if ( spex_cd2a.mode & WN_MODES )
		wn -= spex_cd2a.offset;
	else
		wn = spex_cd2a_wl2Awn( spex_cd2a_Awn2wl( wn ) - spex_cd2a.offset );
	return wn;
}


/*--------------------------------------------------------------*/
/* Converts an absolute wavenumber as seen by the monochromator */
/* to an absolute or relatibe wavenumber (depending on a laser  */
/* line having been set) and as seen by the user                */
/*--------------------------------------------------------------*/

double spex_cd2a_SAwn2UMwn( double wn )
{
	if ( spex_cd2a.mode == WN )
		wn += spex_cd2a.offset;
	else if ( spex_cd2a.mode == WND )
		wn = spex_cd2a.laser_wavenumber - wn;
	else
		wn = spex_cd2a_wl2Awn( spex_cd2a_Awn2wl( wn ) + spex_cd2a.offset );
	return wn;
}


/*------------------------------------------------------------*/
/* Converts an absolute or relative wavenumber (depending on  */
/* the laser line being set) and as seen by the monochromator */
/* to a wavenumber as seen by the user                        */
/*------------------------------------------------------------*/

double spex_cd2a_SMwn2UMwn( double wn )
{
	/* Relative wavenumbers aren't different from the monochromators and
	   users point of view because the correction is part of both the
	   absolute wavenumber and the laser line position */

	if ( spex_cd2a.mode == WND )
		return wn;

	return spex_cd2a_SAwn2UAwn( wn );
}


/*---------------------------------------------------------*/
/* Converts an absolute or relative wavenumber (depending  */
/* on the laser line being set) and as seen by the user to */
/* a wavenumber as seen by the monochromator               */
/*---------------------------------------------------------*/

double spex_cd2a_UMwn2SMwn( double wn )
{
	/* Relative wavenumbers aren't different from the monochromators and
	   users point of view because the correction is part of both the
	   absolute wavenumber and the laser line position */

	if ( spex_cd2a.mode == WND )
		return wn;

	return spex_cd2a_UAwn2SAwn( wn );
}


/*---------------------------------------------------------------------*/
/* Converts a wavelength as seen by the monochromator into an absolute */
/* or relative wavenumber (depending on the laser line being set) as   */
/* seen by the user.                                                   */
/*---------------------------------------------------------------------*/


double spex_cd2a_Swl2UMwn( double wl )
{
	return spex_cd2a_SMwn2UMwn( spex_cd2a_wl2Mwn( wl ) );
}


/*---------------------------------------------------------------------*/
/* Converts an absolute or relative wavenumber (depending on the laser */
/* line being set) and as seen by the user into an absolute wavenumber */
/* as seen by the monochromator.                                       */
/*---------------------------------------------------------------------*/

double spex_cd2a_UMwn2SAwn( double wn )
{
	if ( spex_cd2a.mode == WND )
		return spex_cd2a.laser_wavenumber - wn;

	return spex_cd2a_UAwn2SAwn( wn );
}


/*---------------------------------------------------------------------*/
/* Converts an absolute or relative wavenumber (depending on the laser */
/* line being set) and as seen by the user into a wavelength as seen   */
/* by the monochromator.                                               */
/*---------------------------------------------------------------------*/

double spex_cd2a_UMwn2S2wl( double wn )
{
	return spex_cd2a_Awn2wl( spex_cd2a_UMwn2SAwn( wn ) );
}


/*---------------------------------------------------------*/
/* Converts a wavelength as seen by the monochromator into */
/* an absolute wavenumber as seen by the monochromator     */
/*---------------------------------------------------------*/

double spex_cd2a_Swl2UAwn( double wl )
{
	return spex_cd2a_SAwn2UAwn( spex_cd2a_wl2Awn( wl ) );
}


/*-----------------------------------------------------*/
/* Converts an absolute wavenumber as seen by the user */
/* into a wavelength as seen by the monochromator      */
/*-----------------------------------------------------*/

double spex_cd2a_UAwn2Swl( double wn )
{
	return spex_cd2a_Awn2wl( spex_cd2a_UAwn2SAwn( wn ) );
}


/*---------------------------------------------------------------------*/
/* Converts an absolute or relative wavenumber (depending on the laser */
/* line having been set) and as seen by the user into a wavelength as  */
/* seen by the monochromator.                                          */
/*---------------------------------------------------------------------*/

double spex_cd2a_UMwn2Swl( double wn )
{
	return spex_cd2a_Awn2wl( spex_cd2a_UMwn2SAwn( wn ) );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
