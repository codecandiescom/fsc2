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
	bool is_needed;         /* is the monochromator needed at all? */
	bool is_open;           /* is the device file open ? */
    struct termios *tio;    /* serial port terminal interface structure */
	double wavelength;      /* current wavelength */
	long turret;            /* current turret, range 0-2 */
	long current_grating;   /* current grating, range 0-8 */
	bool use_calib;         /* can calibration info be used ? */
	struct {
		bool is_installed;  /* is grating installed at all ? */
		long grooves;       /* number of grooves per m */
		double blaze;       /* blaze wavelength (negative if not appicable) */
		bool is_calib;      /* is calibration information valid ? */
		double n0;              /* pixel offset of center frequency */
		double inclusion_angle;
		double focal_length;
		double detector_angle;
	} grating[ MAX_GRATINGS ];
};

typedef struct CALIB_PARAMS CALIB_PARAMS;

struct CALIB_PARAMS {
	double d;                    /* grating grooves per millimeter */
	double pixel_width;          /* width of a pixel of the CCD */
	size_t num_values;           /* number of values */
	double *n_exp;               /* array of measured values */
	double *wavelengths;         /* array of wavelengths */
	double *center_wavelengths;  /* array of center wavelengths */
	long *m;                     /* array of diffraction orders */
	int opt;                     /* indicates which parameter is minimized */
	double inclusion_angle;
	double focal_length;
	double detector_angle;
};


#if defined SPECTRAPRO_300I_MAIN

SPECTRAPRO_300I spectrapro_300i;

#else

extern SPECTRAPRO_300I spectrapro_300i;

#endif


/* Default start values for calibrations */

#define DEFAULT_INCLUSION_ANGLE  ( 35.0 * atan( 1.0 ) / 45.0 ) /* 35 degree */
#define DEFAULT_DETECTOR_ANGLE   0.0
#define DEFAULT_DETECTOR_ANGLE_DELTA ( atan( 1.0 ) / 90.0 )    /* 0.5 degree */
#define DEFAULT_EPSILON          1.0e-9


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


FILE *spectrapro_300i_find_calib( char *name );
FILE *spectrapro_300i_open_calib( char *name );
double spectrapro_300i_min( double *x, void *par );
void spectrapro_300i_open( void );
void spectrapro_300i_close( void );
long spectrapro_300i_get_turret( void );
void spectrapro_300i_set_turret( long turret );
long spectrapro_300i_get_grating( void );
void spectrapro_300i_set_grating( long grating );
void spectrapro_300i_get_gratings( void );
void spectrapro_300i_install_grating( char *part_no, long grating );
void spectrapro_300i_uninstall_grating( long grating );
void spectrapro_300i_send( const char *buf );
char *spectrapro_300i_talk( const char *buf, size_t len, long wait_cycles );


void spectrapro_300i_read_calib( FILE *fp, const char *calib_file );


#endif /* ! SPECTRAPRO_300I_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
