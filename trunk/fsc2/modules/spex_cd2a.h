/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


#if ! defined SPEX_CD2A_HEADER
#define SPEX_CD2A_HEADER

#include "fsc2_module.h"


/* Defines for the parity used by the device */

#define NO_PARITY      0
#define ODD_PARITY     1
#define EVEN_PARITY    2
#define ALWAYS_1       3
#define ALWAYS_0       4


/* Defines for the way the device is driven */

#define WAVENUMBER     0
#define WAVELENGTH     1


/* Defines for the units used when the device is wave-length driven */

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


#define WN        1
#define WND       2
#define WL        4
#define WN_MODES  3

/* Include configuration information for the device */

#include "spex_cd2a.conf"


/* Please note: within the module we do all calculations in wave-lengths
   (in m) and thus nearly all data are also stored as wave-lengths. The
   only exception is the position of the laser line, which is stored in
   wave-numbers (i.e. cm^-1). Data passed to the EDL functions must be
   in wave-lengths when the monochromator has a wave-length drive, other-
   wise in wave-numbers. Values returned by the functions follow the same
   pattern. If a laser line position has been set (only possible when in
   wave-number mode) input and output is in relative wave-numbers, i.e.
   wave-number of laser line minus absoulte wave-number.

   Exceptions: monochromator_wavelength() expects and returns data always
               in wave-length units.
			   monochromator_wavenumber() handles data in wave-number units
			   only.
			   monochromator_laser_line() accepts and returns data in
			   absolute wave-number units only.
*/


typedef struct {
	bool is_needed;
	bool is_open;
	int method;
	int units;
	bool has_shutter;

	double lower_limit;              /* wavelength in m */
	double upper_limit;              /* wavelength in m */

	double grooves;                  /* in grooves per m */
	double standard_grooves;         /* in grooves per m */

	bool use_calib;

	double shutter_low_limit;        /* wavelength in m */
	double shutter_high_limit;       /* wavelength in m */
	bool shutter_limits_are_set;

	double wavelength;               /* in m */
	bool is_wavelength;

	double laser_wavenumber;         /* in cm^-1 */

	double start;                    /* in m */
	double step;                     /* in m or cm^-1, depending on mode */
	bool is_init;
	bool in_scan;                    /* set while scanning */

	double mini_step;

	int mode;

	struct termios *tio;             /* serial port terminal interface
										structure */

	bool use_checksum;               /* do we need a checksum in transfers ? */

} SPEX_CD2A;

extern SPEX_CD2A spex_cd2a;


int spex_cd2a_init_hook( void );
int spex_cd2a_test_hook( void );
int spex_cd2a_exp_hook( void );
int spex_cd2a_end_of_exp_hook( void );

Var *monochromator_name( Var *v );
Var *monochromator_init( Var *v );
Var *monochromator_wavelength( Var *v );
Var *monochromator_wavenumber( Var *v );
Var *monochromator_start_scan( Var *v );
Var *monochromator_scan_up( Var *v );
Var *monochromator_laser_line( Var *v );
Var *monochromator_groove_density( Var *v );
Var *monochromator_shutter_limits( Var *v );


void spex_cd2a_init( void );
void spex_cd2a_set_wavelength( void );
void spex_cd2a_halt( void );
void spex_cd2a_start_scan( void );
void spex_cd2a_trigger( void );
void spex_cd2a_set_laser_line( void );
void spex_cd2a_set_shutter_limits( void );
void spex_cd2a_sweep_up( void );
void spex_cd2a_open( void );
void spex_cd2a_close( void );
double spex_cd2a_wn2wl( double wn );
double spex_cd2a_wl2wn( double wl );
double spex_cd2a_wl2mwn( double wl );


#endif /* ! SPEX_CD2A_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
