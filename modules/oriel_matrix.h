/* 
 * $Id$
 *
 * Newport Oriel MMS Spectrometer Driver
 *
 * Notes:
 *    Requires libusb 0.1
 *    Needs root privileges to access USB devices.
 *.   This driver has been modified [hacked :-)] to work with fsc2.
 *
 * Last updated August 19, 2008
 *
 * This driver was developed using the ClearShotII - USB port interface
 * communications and control information specification provided by
 * Centice Corporation (http://www.centice.com)
 *
 * Copyright (c) 2008-2009, Clinton McCrowey <clinton.mccrowey.18@csun.edu>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Clinton McCrowey ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Clinton McCrowey BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#if ! defined ORIEL_MATRIX_H_
#define ORIEL_MATRIX_H_


#include "fsc2_module.h"

#include "oriel_matrix.conf"

#if defined WITH_LIBUSB_0_1
#include <usb.h>
#elif defined WITH_LIBUSB_1_0
#include <libusb-1.0/libusb.h>
#endif


/* Define the maximum size of USB packages to be send and read */

#define USB_WRITE_BUF_SIZE 13
#define USB_READ_BUF_SIZE  64


/* Scheme number, determines the version number of the command send */

#define SCHEME_NUMBER      0x01


/* Defines for endpoints */

#define EP4  4   /* Endpoint 4 (instrument input) */
#define EP6  6   /* Endpoint 6 (CCD image data stream) */
#define EP8  8   /* Endpoint 8 (instrument output) */


/* Defines of commands that can be send to the device */

#define CMD_GET_CCD_TEMPERATURE       0x03
#define CMD_SET_CCD_TEMPERATURE       0x04
#define CMD_OPEN_CCD_SHUTTER          0x07
#define CMD_CLOSE_CCD_SHUTTER         0x08
#define CMD_GET_EXPOSURE_TIME         0x09
#define CMD_SET_EXPOSURE_TIME         0x0A
#define CMD_START_EXPOSURE            0x0B
#define CMD_QUERY_EXPOSURE            0x0C
#define CMD_END_EXPOSURE              0x0D
#define CMD_GET_EXPOSURE              0x0E
#define CMD_GET_RECONSTRUCTION        0x0F
#define CMD_SET_RECONSTRUCTION        0x10
#define CMD_GET_LAST_ERROR            0x11
#define CMD_GET_MODEL_NUMBER          0x12
#define CMD_GET_SERIAL_NUMBER         0x14
#define CMD_GET_SPECTROMETER_INFO     0x16
#define CMD_GET_CLOCK_RATE            0x1B
#define CMD_SET_CLOCK_RATE            0x1C
#define CMD_GET_PIXEL_MODE            0x1D
#define CMD_SET_PIXEL_MODE            0x1E
#define CMD_RESET                     0x1F
#define CMD_GET_CCD_TEMEPRATURE_INFO  0x2A
#define CMD_SET_CCD_TEMEPRATURE_INFO  0x2B


/* Defines of states for shutter related commands */

#define SHUTTER_CLOSED                0x00
#define SHUTTER_OPEN                  0x01
#define SHUTTER_LEAVE_ALONE           0x02


/* Defines of exposure types */

#define EXPOSURE_TYPE_NONE            0x00
#define EXPOSURE_TYPE_LIGHT           0x01
#define EXPOSURE_TYPE_DARK            0x02
#define EXPOSURE_TYPE_REFERENCE       0x03


/* Defines for return values of query of exposre status */

#define EXPOSURE_IS_NOT_AVAILABLE     0x00
#define EXPOSURE_IS_AVAILABLE         0x01
#define EXPOSURE_FAILED               0x02


/* Defines for reconstruction types */

#define RECON_TYPE_LIGHT              0x01
#define RECON_TYPE_DARK               0x02
#define RECON_TYPE_REFERENCE          0x03


/* Defines for A/D clock rates */

#define AD_CLOCK_FREQ_NA              0x00
#define AD_CLOCK_FREQ_DEFAULT         0x01
#define AD_CLOCK_FREQ_150kHz          0x02
#define AD_CLOCK_FREQ_300kHz          0x03
#define AD_CLOCK_FREQ_500kHz          0x04
#define AD_CLOCK_FREQ_1000kHz         0x05
#define AD_CLOCK_FREQ_ERROR           0x06


/* Defines for pixel binning modes */

#define PIXEL_MODE_NA                 0x00
#define PIXEL_MODE_FULL_WELL          0x01
#define PIXEL_MODE_DARK_CURRENT       0x02
#define PIXEL_MODE_LINE_BINNING       0x03
#define PIXEL_MODE_FAILURE            0x04


/* Defines for AFE settings */

#define MAX_AFE_OFFSET                1024
#define MAX_AFE_GAIN                    63


/* Default test values */

#define TEST_EXPOSURE_TIME            0.1    /* 0.1 s */
#define TEST_CAMERA_TEMPERATURE       200    /* 200 K */
#define TEST_PIXEL_WIDTH              32     /* 32 pixel */
#define TEST_PIXEL_HEIGHT             32     /* 32 pixel */


/* Defines of default values that get set */

#define DEFAULT_EXPOSURE_TIME         0.1


/* Structure to hold exposure data */

struct exposure
{
    bool error;                  /* Error status, used to check if exposure
                                    data is valid. If error is true all other
                                    exposure data cannot be considered valid */
  
    unsigned char exposure_type; /* Denotes the type of exposure.
                                    Available types:
                                      0x00: None
                                      0x01: Light Exposure
                                      0x02: Dark Exposure
                                      0x03: Reference Exposure  */
                             
    unsigned int image_size;     /* Length of the image array in bytes
                                    (BBP * pixel height * pixel width) */

    unsigned char * image;       /* Pointer to the image data stored in a
                                    byte array. The data is organized in
                                    the form of row 0 to row pixelWidth
                                    and within each row from column 0 to
                                    column pixelWidth.
                                    **** NOTE **** it is the recieving
                                    function's job to free this pointer's
                                    data back to memory. */
 
    unsigned short int height;   /* Pixel width of the CCD */
    
    unsigned short int width;   /* Pixel height of the CCD */
 
    unsigned char BPP;          /* number of CCD bytes per pixel */
};


/* Structure to hold reconstruction data */

struct reconstruction  
{
    bool error;             /* Error status, used to check if reconstruction
                               data is valid. If error is true all other
                               reconstruction data cannot be considered valid */
 
    unsigned char schema;   /* Denotes the return schema used by the
                               spectrometer This driver only supports schemas
                               0x01 and 0x02. */
 
  unsigned char recon_type;    /* Reconstruction Type - n unsigned char which
                                  denote the type of reconstruction data.
                                  Acceptable values are:
                                  0x01 - Reconstruction from Light Exposure
                                         Requires that both light and dark
                                         exposure have been previously taken
                                         by the spectrometer
                                         prior to this command being used.
                                  0x02 - Reconstruction from Dark Exposure
                                         Requires that a dark exposure has been
                                         taken previously by the spectrometer
                                         prior to this being issued.
                                  0x03 - Reconstruction from Reference Exposure
                                         Requires that both a reference and dark
                                         exposure have been previously taken b
                                         the spectrometer priot to this command
                                         being issued. */
 
    bool saturation;           /* Indicates if the reconstruction was based on
                                  exposure data that had saturated pixels. */
 
    unsigned short int count;  /* The number of array entries that are in the
                                  intensity data array 
                                  **** NOTE **** count has been known at times
                                  to be returned from the spectrometer
                                  inccorrectly, it is often times better to
                                  use response_size instead. */
  
    float * intensity;         /* Array of intensity values generated by the
                                  active reconstruction algorithm. For plotting
                                  on the Y-axis. Values at the tail are padded
                                  with zeros. */
 
    unsigned int response_size; /* Number of bytes in the intensity array of
                                   actual data. */

    /* The following variables are only valid if schema is 0x02
       *** NOTE **** these variables are INVALID if schema is 0x01 */

    bool CCD_IN;                /* CCD Illumination Normalization - Denotes if
                                   illumination normalization of the CCD was
                                   // used to generate the spectral data */

    char recon_mask;            /* Reconstruction Mask - The mask that was
                                   used to generate the spectral data.
                                   Acceptable values are:
                                   0x01 - True
                                   0x02 - False */

    char recon_alg;             /* Reconstruction Mask - The mask that was
                                   used to generate the spectral data.
                                   Acceptable values are:
                                   0x0 - Binary Mask
                                   0x1 - Grey Scale Mask */
};


/* structure to hold temperature information */

struct temperature
{
    bool error;          /* Error status, used to check if temperature data
                            is valid. If error is true all other temperature
                            data cannot be considered valid. */

    bool temp_support;   /* Indicates if temperature regulation is supported.
                            If temperature regulation is not supported all other
                            temperature data cannot be considered valid. */
 
    bool regulation;     /* Indicates if temperature regulation is enabled or
                            disabled */

    float set_point;     /* CCD SetPoint, the current setting of the last
                            configured or default CCD temperature setpoint
                            in degrees C. */

    float current_temp;  /* The current temperature of the CCD in degrees C. */

    bool therm_fault;    /* CCD Therm Fault, indicates an open or short-circuit
                            condition from the CCD thermistor */

    bool temp_lock;      /* CCD temperature lock, indicates if the CCD
                            thermistor temperature is within
                            +/- 0.1 degrees C of the CCD setpoint */
};


/* Structure with some information about the device */

struct ORIEL_MATRIX {
#if defined WITH_LIBUSB_0_1
    usb_dev_handle *udev;        /* USB device hande */
#elif defined WITH_LIBUSB_1_0
    libusb_device_handle *udev;  /* USB device hande */
#endif

    bool has_shutter_support;    /* Do the shutter commands work? */

    double min_exp_time;         /* Minimum and maximum exposure times */
    double max_exp_time;

    double exp_time;             /* Exposure time requested in PREPARATIONS */
    bool is_exp_time;            /* Was exposure time set there? */

    double min_test_exp_time;    /* Minimum and maximum exposure times set */
    double max_test_exp_time;    /* during the test run (in K) */

    bool has_temp_control;       /* Does camera allow temperature control? */
    bool requires_temp_control;  /* Was command for temperature control used? */

    double min_setpoint;         /* Minimum and maximum temperature that can */
    double max_setpoint;         /* be set */

    double temp;                 /* Temperature requested in PREPARATIONS */
    bool is_temp;                /* Was temperature set there? */

    double min_test_temp;        /* Minimum and maximum temperatures set */
    double max_test_temp;        /* during the test run */

    unsigned char ccd_bytes_per_pixel; /* The size in bytes of pixel from the CCD */ 

    unsigned short pixel_width;   /* The height of each pixel */
    unsigned short pixel_height;  /* The width of each pixel */
};


extern struct ORIEL_MATRIX oriel_matrix;


int oriel_matrix_init_hook( void );
int oriel_matrix_test_hook( void );
int oriel_matrix_exp_hook(  void );
int oriel_matrix_end_of_exp_hook( void );


Var_T * ccd_camera_name(          Var_T * /* v */ );
Var_T * ccd_camera_pixel_area(    Var_T * /* v */ );
Var_T * ccd_camera_temperature(   Var_T * /* v */ );
Var_T * ccd_camera_exposure_time( Var_T * /* v */ );
Var_T * ccd_camera_get_spectrum(  Var_T * /* v */ );


void oriel_matrix_init( void );
double oriel_matrix_set_exposure_time( double /* time */ );
double oriel_matrix_get_exposure_time( void );
void oriel_matrix_reset( void );
void oriel_matrix_start_exposure( unsigned char /* shutter_state */,
                                  unsigned char /* exposure_type */ );
unsigned char oriel_matrix_query_exposure( void );
void oriel_matrix_end_exposure( unsigned char /* shutter_state */ );
void oriel_matrix_open_CCD_shutter( void );
void oriel_matrix_close_CCD_shutter( void );
void oriel_matrix_close( void );
unsigned short * oriel_matrix_get_pixel_hw( void );
unsigned char oriel_matrix_get_CCD_BPP( void );
struct exposure * oriel_matrix_get_exposure( void );
void oriel_matrix_set_reconstruction( void );
struct reconstruction * oriel_matrix_get_reconstruction(
                                              unsigned char /* recon_type */ );
unsigned short int oriel_matrix_get_clock_rate( void );
unsigned short int oriel_matrix_set_clock_rate( unsigned short /* freq */ );
unsigned short int oriel_matrix_set_pixel_mode( unsigned short /* mode */ );
unsigned short int oriel_matrix_get_pixel_mode( void );
struct temperature * oriel_matrix_get_CCD_temp( void );
double oriel_matrix_set_CCD_temp( double /* temperature */ );
void oriel_matrix_disable_CCD_temp_regulation( void );
void oriel_matrix_set_AFE_parameters( unsigned short /* offset */,
                                      unsigned short /* gain */ );
unsigned short int * oriel_matrix_get_AFE_parameters( void );


#endif /* ! ORIEL_MATRIX_H_ */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
