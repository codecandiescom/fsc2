/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
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


#if ! defined SPEX_CD2A_HEADER
#define SPEX_CD2A_HEADER


#include "fsc2_module.h"
#include "serial.h"


/* Define the following for test purposes only where no real communication
   with the monochromator is supposed to happen */

/*
#define SPEX_CD2A_TEST
*/

/* Defines for the parity used by the device */

#define NO_PARITY      0
#define ODD_PARITY     1
#define EVEN_PARITY    2
#define ALWAYS_1       3
#define ALWAYS_0       4


/* Defines for the way the device is driven */

#define WAVENUMBER     0
#define WAVELENGTH     1


/* Defines for the units used when the device is wavelength driven */

#define NANOMETER      0
#define ANGSTROEM      1


/* Defines for the motor type */

#define MSD            0
#define LSD            1


/* Defines for the data transfer protocol */

#define STANDARD       0
#define DATALOGGER     1


#define NO_CHECKSUM    0
#define WITH_CHECKSUM  1

#define NO_LINEFEED    0
#define WITH_LINEFEED  1


#define WN_ABS    1         /* absolute wavenumber mode */
#define WN_REL    2         /* relative wavenumber mode */
#define WL        4         /* wavelength mode */
#define WN_MODES  3

/* Include configuration information for the device */

#include "spex_cd2a.conf"


/* Please note: within the module we do all calculations in wavelengths
   (in m) and thus nearly all data are also stored as wavelengths. The
   only exception is the position of the laser line, which is stored in
   wavenumbers (i.e. cm^-1) and the step width. Data passed to the EDL
   functions must be in wavelengths when the monochromator has a wave-
   length drive, otherwise in wavenumbers. Values returned by the
   functions follow the same pattern. If a laser line position has been
   set (only possible when in wavenumber mode) input and output is in
   relative wavenumbers, i.e. wavenumber of laser line minus absoulte
   wavenumber.

   Exceptions: monochromator_wavelength() expects and returns data always
               in wavelength units.
               monochromator_wavenumber() handles data in wavenumber units
               only.
               monochromator_laser_line() accepts and returns data in
               absolute wavenumber units only.
               monochromator_wavelength_axis() returns data always in
               wavelength units
               monochromator_wavenumber_axis() returns data always in
               wavenumber units
*/


/* All wavelengths and -numbers stored in the following structure are (as
   far as applicable) in monochromator units, i.e. without corrections due
   to the offset value also stored in the structure. As far as possible all
   data are stored in wavelength units, exceptions being the laser line
   position (which only makes sense at all in wavenumber mode) and the
   scan step width and the minimum value for a scan step. */

struct Spex_CD2A {
    bool is_needed;
    bool is_open;
    int method;
    int units;
    bool has_shutter;

    int mode;                       /* wavelength (WL) or wavenumber
                                       absolute (WN_ABS), relative (WN_REL) */

    double wavelength;              /* in m */
    bool is_wavelength;

    double laser_line;              /* in cm^-1 (wavenumber mode only) */

    double scan_start;              /* in m */
    double scan_step;               /* in m or cm^-1, depending on mode */
    bool scan_is_init;
    bool in_scan;                   /* set while scanning */

    double offset;                  /* in m or abs. cm^-1, depending on mode */

    double pixel_diff;              /* in m */

    double mini_step;               /* in m or cm^-1, depending on mode */

    double lower_limit;             /* wavelength in m */
    double upper_limit;             /* wavelength in m */

    double grooves;                 /* in grooves per m */
    double standard_grooves;        /* in grooves per m */

    double shutter_low_limit;       /* wavelength in m */
    double shutter_high_limit;      /* wavelength in m */
    bool shutter_limits_are_set;

    struct termios *tio;            /* serial port terminal interface
                                       structure */

    bool data_format;               /* either STANDARD or DATALOGGER */
    bool use_checksum;              /* do we need a checksum in transfers ? */
    bool sends_lf;

    bool fatal_error;               /* set on exceptions etc. */
};

extern struct Spex_CD2A spex_cd2a;


/* Functions from spex_cd2a.c */

int spex_cd2a_init_hook(        void );
int spex_cd2a_test_hook(        void );
int spex_cd2a_exp_hook(         void );
void spex_cd2a_child_exit_hook( void );
int spex_cd2a_end_of_exp_hook(  void );

Var_T * monochromator_name(                   Var_T * /* v */ );
Var_T * monochromator_wavenumber_scan_limits( Var_T * /* v */ );
Var_T * monochromator_wavelength_scan_limits( Var_T * /* v */ );
Var_T * monochromator_scan_setup(             Var_T * /* v */ );
Var_T * monochromator_wavelength(             Var_T * /* v */ );
Var_T * monochromator_wavenumber(             Var_T * /* v */ );
Var_T * monochromator_offset(                 Var_T * /* v */ );
Var_T * monochromator_start_scan(             Var_T * /* v */ );
Var_T * monochromator_scan_step(              Var_T * /* v */ );
Var_T * monochromator_laser_line(             Var_T * /* v */ );
Var_T * monochromator_groove_density(         Var_T * /* v */ );
Var_T * monochromator_shutter_limits(         Var_T * /* v */ );
Var_T * monochromator_calibrate(              Var_T * /* v */ );
Var_T * monochromator_wavelength_axis(        Var_T * /* v */ );
Var_T * monochromator_wavenumber_axis(        Var_T * /* v */ );


/* Functions from spex_cd2a_ll.c */

void spex_cd2a_init( void );

void spex_cd2a_set_wavelength( void );

void spex_cd2a_halt( void );

void spex_cd2a_scan_start( void );

void spex_cd2a_scan_step( void );

void spex_cd2a_start_scan( void );

void spex_cd2a_trigger( void );

void spex_cd2a_set_laser_line( void );

void spex_cd2a_set_shutter_limits( void );

void spex_cd2a_sweep_up( void );

void spex_cd2a_open( void );

void spex_cd2a_close( void );


/* Functions from spex_cd2a_util.c */

bool spex_cd2a_read_state( void );

bool spex_cd2a_store_state( void );

double spex_cd2a_wl2wn( double /* wl */ );

double spex_cd2a_wn2wl( double /* wn */ );

double spex_cd2a_wl2Uwl( double /* wl */ );

double spex_cd2a_Uwl2wl( double /* wl */ );

double spex_cd2a_wn2Uwn( double /* wn */ );

double spex_cd2a_Uwn2wn( double /* wn */ );

double spex_cd2a_wn2Uwl( double /* wn */ );

double spex_cd2a_Uwl2wn( double /* wl */ );

double spex_cd2a_wl2Uwn( double /* wl */ );

double spex_cd2a_Uwn2wl( double /* wn */ );

double spex_cd2a_wl2mu( double /* wl */ );


#define SPEX_CD2A_THROW( x )  do { spex_cd2a.fatal_error = SET;      \
                                   THROW( x );                       \
                              } while ( 0 )

#define SPEX_CD2A_RETHROW( )  do { spex_cd2a.fatal_error = SET;      \
                                   RETHROW( );                       \
                              } while ( 0 )


#define SPEX_CD2A_ASSERT( x ) do { if ( ! ( x ) )                    \
                                       spex_cd2a.fatal_error = SET;  \
                                   fsc2_assert( x );                 \
                               } while( 0 )

#define SPEX_CD2A_IMPOSSIBLE( ) do { spex_cd2a.fatal_error = SET;    \
                                     fsc2_impossible( );             \
                                } while( 0 )


#endif /* ! SPEX_CD2A_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
