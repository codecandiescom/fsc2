/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2005 Jens Thoms Toerring
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


#if ! defined SPECTRAPRO_300I_HEADER
#define SPECTRAPRO_300I_HEADER

#include "fsc2_module.h"

/* Include configuration information for the device */

#include "spectrapro_300i.conf"


struct SPECTRAPRO_300I {
	bool is_needed;         /* is the monochromator needed at all? */
	bool is_open;           /* is the device file open ? */
    struct termios *tio;    /* serial port terminal interface structure */
	double wavelength;      /* current wavelength */
	bool is_wavelength;     /* if wavelength got set in PREPARATIONS section */
	long tn;                /* current turret number, range 0-2 */
	long current_gn;        /* current grating number, range 0-8 */
	bool use_calib;         /* can calibration info be used ? */
	struct {
		bool is_installed;  /* is grating installed at all ? */
		long grooves;       /* number of grooves per m */
		double blaze;       /* blaze wavelength (negative if not appicable) */
		bool is_calib;      /* is calibration information valid ? */
		double init_offset;
		double init_adjust;
		double inclusion_angle;
		double focal_length;
		double detector_angle;
		bool used_in_test;
		bool installed_in_test;
	} grating[ MAX_GRATINGS ];
};

typedef struct Calib_Params Calib_Params_T;

struct Calib_Params {
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


extern struct SPECTRAPRO_300I spectrapro_300i;


/* Default start values for calibrations */

#define DEFAULT_INCLUSION_ANGLE      ( 30.3 * atan( 1.0 ) / 45.0 )
#define DEFAULT_DETECTOR_ANGLE       0.0
#define DEFAULT_DETECTOR_ANGLE_DELTA ( atan( 1.0 ) / 180.0 )  /* 0.25 degree */
#define DEFAULT_EPSILON              1.0e-9

#define INIT_OFFSET                   1536000
#define INIT_OFFSET_RANGE             ( 1200000.0 * 2500.0 )
#define INIT_ADJUST                   1000000
#define INIT_ADJUST_RANGE              100000


int spectrapro_300i_init_hook( void );
int spectrapro_300i_exp_hook( void );
int spectrapro_300i_end_of_exp_hook( void );

Var_T *monochromator_name( Var_T *v );
Var_T *monochromator_turret( Var_T *v );
Var_T *monochromator_grating( Var_T *v );
Var_T *monochromator_wavelength( Var_T *v );
Var_T *monochromator_wavenumber( Var_T *v );
Var_T *monochromator_install_grating( Var_T *v );
Var_T *monochromator_groove_density( Var_T *v );
Var_T *monochromator_calibrate( Var_T *v );
Var_T *monochromator_wavelength_axis( Var_T * v );
Var_T *monochromator_wavenumber_axis( Var_T * v );
Var_T *monochromator_calc_wavelength( Var_T *v );
Var_T *monochromator_load_calibration( Var_T * v );
Var_T *monochromator_set_calibration( Var_T *v );
Var_T *monochromator_zero_offset( Var_T *v );
Var_T *monochromator_grating_adjust( Var_T *v );


FILE *spectrapro_300i_find_calib( char *name );
FILE *spectrapro_300i_open_calib( char *name );
double spectrapro_300i_min( double *x, void *par );
void spectrapro_300i_open( void );
void spectrapro_300i_close( void );
double spectrapro_300i_get_wavelength( void );
void spectrapro_300i_set_wavelength( double wavelength );
long spectrapro_300i_get_turret( void );
void spectrapro_300i_set_turret( long tn );
long spectrapro_300i_get_grating( void );
void spectrapro_300i_set_grating( long gn );
void spectrapro_300i_get_gratings( void );
long spectrapro_300i_get_offset( long gn );
void spectrapro_300i_set_offset( long gn, long offset );
long spectrapro_300i_get_adjust( long gn );
void spectrapro_300i_set_adjust( long gn, long adjust );
void spectrapro_300i_install_grating( long gn, const char *part_no );
void spectrapro_300i_uninstall_grating( long gn );
double spectrapro_300i_wl2wn( double wl );
double spectrapro_300i_wn2wl( double wn );


void spectrapro_300i_read_calib( FILE *fp, const char *calib_file );


#endif /* ! SPECTRAPRO_300I_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
