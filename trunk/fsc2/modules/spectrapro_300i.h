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


#if ! defined SPECTRAPRO_300I_HEADER
#define SPECTRAPRO_300I_HEADER

#include "fsc2_module.h"

/* Include configuration information for the device */

#include "spectrapro_300i.conf"



typedef struct SPECTRAPRO_300I SPECTRAPRO_300I;

struct SPECTRAPRO_300I {
	bool is_needed;         /* is the gaussmter needed at all? */
    struct termios *tio;    /* serial port terminal interface structure */
	double wavelength;
	long turret;            /* current turret, range 0-2 */
	long max_gratings;      /* current grating, range 0-2 */
	long current_grating;
	bool use_calib;
	struct {
		bool is_installed;
		long grooves;       /* number of grooves per m */
		double blaze;       /* blaze wavelength (negative if not appicable) */
		bool is_calib;
		double n0;
		double inclusion_angle;
		double focal_length;
		double detector_angle;
	} grating[ MAX_GRATINGS ];
};


#if defined SPECTRAPRO_300I_MAIN

SPECTRAPRO_300I spectrapro_300i;

#else

extern SPECTRAPRO_300I spectrapro_300i;

#endif


int spectrapro_300i_init_hook( void );
int spectrapro_300i_exp_hook( void );
int spectrapro_300i_end_of_exp_hook( void );
void spectrapro_300i_exit_hook( void );
Var *monochromator_name( Var *v );
Var *monochromator_turret( Var *v );
Var *monochromator_grating( Var *v );
Var *monochromator_wavelength( Var *v );
Var *monochromator_install_grating( Var *v );
Var *monochromator_groove_density( Var *v );
Var *monochromator_calibration( Var *v );
Var *monochromator_load_calibration( Var * v );
Var *monochromator_wavelength_axis( Var * v );


void spectrapro_300i_read_calib( FILE *fp, const char *calib_file );


#endif /* ! SPECTRAPRO_300I_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
