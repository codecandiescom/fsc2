/*
 * $Id$
 *
 * Newport Oriel MMS Spectrometer Driver
 *
 * Notes:
 *    File requires oriel_matrix.h
 *    Tested only on x86 machines, may not work on other machines.
 *    (Will not work on 64-bit machines as-is)
 *    Needs root privileges to access USB devices
 *.   This driver has been modified [hacked :-)] to work with fsc2.
 *
 * Last updated August 19, 2008
 *
 * This driver was developed using the ClearShotII - USB port interface
 * communications and control information specification provided by Centice
 * Corporation (http://www.centice.com)
 *
 * Copyright (c) 2008, Clinton McCrowey <clinton.mccrowey.18@csun.edu>.
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


#include "oriel_matrix.h"


static void oriel_matrix_get_info( void );
static unsigned char * oriel_matrix_communicate( unsigned char cmd,
                                                 ... );
static unsigned short int device_2_ushort( unsigned char *src );
static void ushort_2_device( unsigned char * dest,
                             unsigned short  val );
static short int device_2_short( unsigned char * src );
static unsigned int device_2_uint( unsigned char * src );
static void uint_2_device( unsigned char * dest,
                           unsigned int    val );
static float device_2_float( unsigned char * src );
static void float_2_device( unsigned char * dest,
                            float           val );

/* Commented sinc only used in development/debugging */

#if 0
static char * oriel_matrix_get_model_number( void );
static char * oriel_matrix_get_serial_number( void );
static void oriel_matrix_print_info( void );
static unsigned int * oriel_matrix_get_last_error( void );
#endif


/*----------------------------------------------------------------*
 * This function detects and initializes the Newport spectrometer
 * and must be called before it can be used. All other functions
 * will fail if called upon before calling this function.
 *
 * Input: None.
 *
 * Return Value: none, ob failure an exception is thrown
 *----------------------------------------------------------------*/

void
oriel_matrix_init( void )
{
    struct usb_bus *busses,
                   *bus;
    struct usb_device *dev = NULL;
  

    fsc2_assert( oriel_matrix.udev == NULL );

    raise_permissions( );

    busses = usb_get_busses( );

    /* Scan each device on each bus for the Newport MMS Spectrometer */

    for ( bus = busses; bus && dev == NULL; bus = bus->next )
        for ( dev = bus->devices; dev; dev=dev->next )
            if (    dev->descriptor.idVendor  == VENDOR_ID
                 && dev->descriptor.idProduct == PRODUCT_ID )
                break;

    if ( ! dev )
    {
        lower_permissions( );
        print( FATAL, "Device not found on USB.\n" );
        THROW( EXCEPTION );
    }

    if ( ! ( oriel_matrix.udev = usb_open( dev ) ) )
    {
        lower_permissions( );
        print( FATAL, "Can't open connection to device.\n" );
        THROW( EXCEPTION );
    }

    if ( usb_set_configuration( oriel_matrix.udev, 1 ) < 0 )
    {
        lower_permissions( );
        print( FATAL, "Can't set active configuration.\n" );
        THROW( EXCEPTION );
    }

    if ( usb_claim_interface( oriel_matrix.udev, 0 ) < 0 )
    {
        lower_permissions( );
        print( FATAL, "Can't claim interface.\n" );
        THROW( EXCEPTION );
    }

    if ( usb_set_altinterface( oriel_matrix.udev, 0 ) < 0 )
    {
        lower_permissions( );
        print( FATAL, "Can't set alternate interface.\n" );
        THROW( EXCEPTION );
    }

    lower_permissions( );

    oriel_matrix_get_info( );
    oriel_matrix_set_reconstruction( );
}


/*----------------------------------------------------------------------*
 * Function to get some information about the limitations of the device
 *----------------------------------------------------------------------*/

static void
oriel_matrix_get_info( void )
{
    unsigned char *readbuf =
                         oriel_matrix_communicate( CMD_GET_SPECTROMETER_INFO );

    oriel_matrix.has_shutter_support = readbuf[ 13 ];

    oriel_matrix.min_exp_time = 1.0e-3 * device_2_uint( readbuf + 14 );
    oriel_matrix.max_exp_time = 1.0e-3 * device_2_uint( readbuf + 18 );

    oriel_matrix.has_temp_control = readbuf[ 24 ];

    oriel_matrix.min_setpoint = device_2_short( readbuf + 25 ) + 273.15;
    oriel_matrix.max_setpoint = device_2_short( readbuf + 27 ) + 273.15;

    oriel_matrix.ccd_bytes_per_pixel = readbuf[ 11 ] / 8;

    oriel_matrix.pixel_width = device_2_ushort( readbuf + 7 );
    oriel_matrix.pixel_height = device_2_ushort( readbuf + 9 );

}


/*----------------------------------------------------------------------*
 * This function assigns a new image exposure time to the spectrometer.
 * If the exposure time is below that of the spectrometer's minimum
 * exposure time, the exposure time will be set to that of the
 * spectrometer's minimum exposure time. Likewise If the exposure time
 * is above that of the spectrometer;s maximum exposure time, the
 * exposure time will be set to that of the spectrometer's maximum
 * exposure time.
 *
 * Input:
 *   double time: the time that should be set (seconds)
 *
 * Return value: if successful returns a double of the exposure time
 * (in seconds) else an exception is thrown
 *----------------------------------------------------------------------*/

double
oriel_matrix_set_exposure_time( double exp_time )
{
    oriel_matrix_communicate( CMD_SET_EXPOSURE_TIME, exp_time );
    return oriel_matrix_get_exposure_time( );
}


/*---------------------------------------------------------------*
 * This function fetches the spectrometer's currently configured
 * exposure time. The exposure time returned by the command will
 * be within or equal to the minimum and maximum exposure times
 * that are allowed by the spectrometer.
 *
 * Input: None
 *
 * Return value: if successful returns a double of the exposure
 * time (in seconds) or else an exception is thrown
 *---------------------------------------------------------------*/

double
oriel_matrix_get_exposure_time( void )
{
    unsigned char *readbuf = oriel_matrix_communicate( CMD_GET_EXPOSURE_TIME );

    return device_2_float( readbuf + 7 );
}


/*---------------------------------------------------------------------*
 * This function will reset the spectrometer to it's default settings.
 * This function may/will affect the following: CCD temperature
 * regulation and set point, shutter, exposure time, reconstruction
 * mode, clock rate, pixel mode, on/off status of laser, laser power
 * setting(s), and temperature regulation and set point of lasers.
 *
 * *** NOTE **** This function can sometimes cause problems and it
 * may be better to use the oriel_matrix_close() function instead.
 *
 * Input: none
 *
 * Return value: none - on failure an exception gets thrown
 *---------------------------------------------------------------------*/

void
oriel_matrix_reset( void )
{
    oriel_matrix_communicate( CMD_RESET );
    fsc2_usleep( 5000000, UNSET );
}


/*-------------------------------------------------------------------------*
 * This command informs the spectrometer to start a camera exposure.
 * The exposure that is taken will have a time duration equal to that
 * of the last configured exposure time. The exposure that is taken
 * can either use the existing shutter setting or force the state of
 * the shutter. Shutter support can be determined by calling the
 * print_info() function.
 *
 * Inputs:
 *   unsigned char shutter_state:
 *         an 8-bit unsigned integer which defines what should occur
 *         with the spectrometer's camera shutter. Acceptable values are:
 *               0x00 - close shutter
 *               0x01 - open shutter
 *               0x02 - leave shutter in its current state
 *
 *   unsigned char exposure_type:
 *         an 8-bit unsigned integer which defines the type of exposure
 *         that is to be taken. Acceptable values are:
 *               0x00 - none
 *               0x01 - light exposure
 *               0x02 - dark exposure
 *               0x03 - reference exposure
 * 
 * Return value: none, on faile an exception is thrown
 *-------------------------------------------------------------------------*/

void
oriel_matrix_start_exposure( unsigned char shutter_state,
                             unsigned char exposure_type )
{
    fsc2_assert( shutter_state <= SHUTTER_LEAVE_ALONE );
    fsc2_assert( exposure_type <= EXPOSURE_TYPE_REFERENCE );

    oriel_matrix_communicate( CMD_START_EXPOSURE, shutter_state,
                              exposure_type );
}


/*---------------------------------------------------------------------*
 * Returns the current state of the spectrometer's started exposure.
 * Repeated calls to this command will allow an outside process to
 * determine if a requested exposure has been taken by the
 * spectrometer's camera. This command will only successfully
 * complete if it is called between both oriel_matrix_start_exposure()
 * and oriel_matrix_end_exposure().
 *
 * Input: None
 * 
 * Return value: if successful an unsigned char which indicates if
 *               the last requested exposure is available or not.
 *               Returned values are:
 *                0x00 - not available
 *                0x01 - available
 * On detection of a failure an exception will gets thrown.
 *---------------------------------------------------------------------*/

unsigned char
oriel_matrix_query_exposure( void )
{
    unsigned char *readbuf;


    readbuf = oriel_matrix_communicate( CMD_QUERY_EXPOSURE );

    if ( readbuf[ 7 ] == EXPOSURE_FAILED )
    {
        print( FATAL, "Exposur failed, aborting experiment.\n" );
        THROW( EXCEPTION );
    }

    return readbuf[ 7 ];
}


/*-----------------------------------------------------------------------*
 * This function ends the use of the spectrometer's camera in regards
 * to acquiring an exposure. This function should be called only after
 * calls to Ystart_exposure() and when all spectrometer exposure related
 * commands have completed. Execution of this function will cause all
 * future oriel_matrix_query_exposure() function commands to fail until
 * the next oriel_matrix_start_exposure() command successfully completes.
 *
 * Input:
 *   unsigned char shutter_state:
 *         an unsigned integer which defines what
 *         should occur with the spectrometer's camera shutter.
 *         Possible values are:
 *            0x00 - close shutter
 *            0x01 - open shutter
 *            0x02 - leave shutter in its current state
 * 
 * Return value: none, on failure an exception gets thrown
 *-----------------------------------------------------------------------*/

void
oriel_matrix_end_exposure( unsigned char shutter_state )
{
    oriel_matrix_communicate( CMD_END_EXPOSURE, shutter_state );
}


/*----------------------------------------------------------*
 * This function will open the spectrometer's CCD shutter.
 * This function will do nothing if the spectrometer does
 * not support a shutter. Shutter support can be determined
 * by the use of print_info() function.
 *
 * Inputs: None
 * 
 * Return value: none, throws exception of failure
 *----------------------------------------------------------*/

void
oriel_matrix_open_CCD_shutter( void )
{
    oriel_matrix_communicate( CMD_OPEN_CCD_SHUTTER );
}


/*----------------------------------------------------------*
 * This function will close the spectrometer's CCD shutter.
 * This function will do nothing if the spectrometer does
 * not support a shutter. Shutter support can be determined
 * by the use of print_info() function.
 *
 * Inputs: None
 * 
 * Return value: none, throws an exception on failure
 *----------------------------------------------------------*/

void
oriel_matrix_close_CCD_shutter( void )
{
    oriel_matrix_communicate( CMD_CLOSE_CCD_SHUTTER );
}


/*-----------------------------------------------------*
 * This function will reset the device and release the
 * spectrometer's udev handle.
 *
 * Inputs: None
 * 
 * Return value: none
 *-----------------------------------------------------*/

void
oriel_matrix_close( void )
{
    oriel_matrix_close_CCD_shutter( );
    raise_permissions( );
    usb_reset( oriel_matrix.udev );
    usb_close( oriel_matrix.udev );
    lower_permissions( );
    oriel_matrix.udev = NULL;
}


/*----------------------------------------------------------------*
 * Returns the pixel hieght and width of the spectrometer in use.
 *
 * Input: None
 *
 * Return value: a unsigned short integer array which contains
 *               the following inormation:
 *                 index = 0: pixel width
 *                 index = 1; pixel height
 *               Throws an exception on failure
 *----------------------------------------------------------------*/

unsigned short *
oriel_matrix_get_pixel_hw( void )
{
    unsigned char *readbuf;
    static unsigned short pixelSize[ 2 ];

    readbuf = oriel_matrix_communicate( CMD_GET_SPECTROMETER_INFO );

    pixelSize[ 0 ] = device_2_ushort( readbuf + 7 );
    pixelSize[ 1 ] = device_2_ushort( readbuf + 9 );

    if ( pixelSize[ 0 ] == 0 || pixelSize[ 1 ] == 0 )
    {
        print( FATAL, "Device returns invalid pixel width or height data.\n" );
        THROW( EXCEPTION );
    }

    return pixelSize;
}


/*----------------------------------------------------------------*
 * This function returns an unsigned char which indicates the
 * number of CCD byte per pixel
 *
 * Input: None
 *
 * return value: unsigned char which is the number of CCD bytes
 *               per pixel. Function throws execption on failure.
 *----------------------------------------------------------------*/

unsigned char
oriel_matrix_get_CCD_BPP( void )
{
    unsigned char * readbuf =
                         oriel_matrix_communicate( CMD_GET_SPECTROMETER_INFO );

    return readbuf[ 11 ] / 8;
}


/*---------------------------------------------------------------------------*
 * Returns a struct exposure with image data.
 *
 * Input: None
 *
 * Return value: pointer to  struct exposure with image data. On
 *               failure an exception gets thrown.
 *
 * **** NOTE **** This function has not been thoroughly tested and might
 *                not be implemented correctly. This however shouldn't be
 *                a problem for most users since calling this function will
 *                only return the CCD raw image data which unless you have 
 *                access to centice's proprietary reconstruction algorithm
 *                is useless. Instead most users can take advantage of the
 *                spectrometer's on-board reconstruction processing (use
 *                get_info() to test if on-board reconstruction is supported
 *                on your hardware.) via the get_reconstruction() function.
 *---------------------------------------------------------------------------*/

struct exposure *
oriel_matrix_get_exposure( void )
{
    unsigned char *readbuf;
    static struct exposure pix;
    size_t i;

    pix.width = oriel_matrix.pixel_width;
    pix.height = oriel_matrix.pixel_height;

    pix.BPP = oriel_matrix.ccd_bytes_per_pixel;

    readbuf = oriel_matrix_communicate( CMD_GET_EXPOSURE );

    pix.error = UNSET;
    pix.exposure_type = readbuf[ 7 ];
    pix.image_size = device_2_uint( readbuf + 8 );
    pix.image = T_malloc( pix.image_size );

    raise_permissions( );

    /* Read in the data in chunks of USB_READ_BUF_SIZE bytes since the device
       always sends them that way */ 

    for ( i = 0; i < pix.image_size; i += USB_READ_BUF_SIZE )
        if ( usb_bulk_read( oriel_matrix.udev, EP6,
                            ( char * ) ( pix.image + i ),
                            USB_READ_BUF_SIZE, 0 ) <= 0 )
        {
            lower_permissions( );
            print( FATAL, "Failed to get exposure.\n" );
            T_free( pix.image );
            THROW( EXCEPTION );
        }

    lower_permissions( );
    return &pix;
}


/*------------------------------------------------------------------------*
 * This function sets the reconstruction algorithm that will be used to
 * geternate spectra data on an camera exposure. This command will only
 * successfully complete if it is called outside a
 * oriel_matrix_start_exposure() and oriel_matrix_end_exposure() function
 * calls
 * 
 * Inputs: None
 *
 * Return value: none, throws exeption on failure
 *
 * *** NOTE *** This function i never used and the value passed
 *              with the command as the reconstruction type looks
 *              fishy.
 *------------------------------------------------------------------------*/

void
oriel_matrix_set_reconstruction( void )
{
    oriel_matrix_communicate( CMD_SET_RECONSTRUCTION, 0x00 );
}


/*---------------------------------------------------------------------*
 * This command fetches the digital spectra reconstruction of a
 * captured exposure. This command should only be used outside
 * the use of the oriel_matrix_start_exposure() and
 * oriel_matrix_end_exposure() functions.
 *
 * Input:
 *   unsigned char recon_type:
 *         Reconstruction Type - Denotes the type of reconstruction
 *         data to retrieve. Acceptable values are:
 *            0x01 - Reconstruction from Light Exposure - Requires
 *                   that both light and dark exposure have been
 *                   previously taken by the spectrometer prior to
 *                   this command being used.
 *            0x02 - Reconstruction from Dark Exposure - Requires that
 *                   a dark exposure has been taken previously by the
 *                   spectrometer prior to this being issued.
 *            0x03 - Reconstruction from Reference Exposure - Requires
 *                   that both a reference and dark exposure have been
 *                   previously taken b the spectrometer priot to this
 *                   command being issued.
 *
 * Return value: pointer to struct reconstruction object, on failure
 *               an exception gets thrown
 *---------------------------------------------------------------------*/

struct reconstruction *
oriel_matrix_get_reconstruction( unsigned char recon_type )
{
    unsigned char *readbuf;
    static struct reconstruction recon;    /* reconstruction struct */
    size_t i;


    fsc2_assert(    recon_type >= RECON_TYPE_LIGHT
                 && recon_type <= RECON_TYPE_REFERENCE );

    readbuf = oriel_matrix_communicate( CMD_GET_RECONSTRUCTION, recon_type );

    recon.error = UNSET;
    recon.saturation = readbuf[ 8 ];
    recon.count = device_2_ushort( readbuf + 9 );
    recon.schema = readbuf[ 5 ];
    recon.recon_type = readbuf[ 7 ];

    /* This is not the actual number of elements in the array since the
       last few elements are padded with 0s */

    recon.response_size = device_2_uint( readbuf + 1 ) - USB_READ_BUF_SIZE;

    recon.intensity = T_malloc( recon.response_size );

    if ( recon.schema == 0x02 )
    {
        recon.CCD_IN = readbuf[ 11 ];
        recon.recon_mask = readbuf[ 12 ];
        recon.recon_alg = readbuf[ 13 ];
    }
      
    raise_permissions( );

    /* Read in the data in chunks of USB_READ_BUF_SIZE bytes since the device
       always sends them that way */ 

    for ( i = 0; i < recon.response_size; i += USB_READ_BUF_SIZE )
        if ( usb_bulk_read( oriel_matrix.udev, EP8,
                            ( char * ) recon.intensity + i,
                            USB_READ_BUF_SIZE, 0 ) <= 0 )
        {
            lower_permissions( );
            print( FATAL, "Failed to get reconstruction.\n" );
            T_free( recon.intensity );
            THROW( EXCEPTION );
        }

    lower_permissions( );

    return &recon;
}


/*-------------------------------------------------------------------*
 * This function returns the clock frequency that the spectrometer's
 * camera uses to convert analog data to digital data.
 *
 * Input Value: None
 *
 * Return Value: an unsigned short int which specifies the clock
 *               frequncy of the A/D converter.
 *               Possible return values:
 *               0x00 = N/A
 *               0x01 = Default
 *               0x02 = 150 KHz
 *               0x03 = 300 KHz
 *               0x04 = 500 KHz
 *               0x05 = 1000 KHz
 *               0x06 = Error getting clock rate
 *-------------------------------------------------------------------*/

unsigned short int
oriel_matrix_get_clock_rate( void )
{
    unsigned char *readbuf = oriel_matrix_communicate( CMD_GET_CLOCK_RATE );

    return device_2_ushort( readbuf + 7 );
}


/*------------------------------------------------------------------*
 * This function is used to set the clock frequency that the
 * spectrometer's camera uses to convert analog data to digital
 * data. Faster clock rates allow for image data to be processed
 * faster but with increased electronic noise within the captured
 * CCD image. Slower clock rates lengthen the image processing time
 * but results in a lowered electronic noise withing the captured
 * CCD image.
 *
 * Input:
 *   unsigned short freq: Specifies the clock frequency of the A/D
 *   converter.
 *                        Acceptable values are:
 *                        0x00 = N/A
 *                        0x01 = Default
 *                        0x02 = 150 KHz
 *                        0x03 = 300 KHz
 *                        0x04 = 500 KHz
 *                        0x05 = 1000 KHz
 * Return value: unsigned short int with the clock rate.
 *               Function throws exception on failure
 *------------------------------------------------------------------*/

unsigned short int
oriel_matrix_set_clock_rate( unsigned short freq )
{
    fsc2_assert( freq <= AD_CLOCK_FREQ_1000kHz );

    oriel_matrix_communicate( CMD_SET_CLOCK_RATE, freq );
    return oriel_matrix_get_clock_rate( );
}


/*--------------------------------------------------------------------*
 * This function is used to set the pixel binning mode that the
 * spectrometer's camera will use with its processing. Pixel
 * binning is where the physical light wells (pixels) on the
 * spectrometer's CCD are grouped together for processing as a
 * single pixel.
 *
 * Input:
 *   unsigned short int mode:
 *             Specifies the pixel binning mode to be used.
 *                Acceptable values are:
 *                0x00 = N/A
 *                0x01 = Full Well
 *                0x02 = Dark Current
 *                0x03 = Line Binning
 * 
 * Return value: if sucessful returns an unsigned short int which
 *               confirms the pixel binning mode, were 0x04 signifies
 *               an error setting the pixel binning mode. On failure
 *               an exception gets thrown
 *--------------------------------------------------------------------*/

unsigned short int
oriel_matrix_set_pixel_mode( unsigned short mode )
{
    fsc2_assert( mode <= PIXEL_MODE_LINE_BINNING );

    oriel_matrix_communicate( CMD_SET_PIXEL_MODE, mode );
    return oriel_matrix_get_pixel_mode( );
}


/*----------------------------------------------------------------------*
 * This function returns the pixel binning mode that the spectrometer's
 * camera uses. Pixel binning is where physical light wells (pixels) on
 * the spectrometer's CCD are grouped together for processing as a
 * single pixel.
 *
 * Input Value: None
 *
 * Return Value: an unsigned short int which specifies the pixel
 *               binning mode of the CCD.
 *               Possible return values:
 *               0x00 = N/A
 *               0x01 = Full Well
 *               0x02 = Dark Current
 *               0x03 = Line Binning
 *               0x04 = Error getting pixel binning mode
 *----------------------------------------------------------------------*/

unsigned short int
oriel_matrix_get_pixel_mode( void )
{
    unsigned char * readbuf = oriel_matrix_communicate( CMD_GET_PIXEL_MODE );

    return device_2_ushort( readbuf + 7 );
}


/*----------------------------------------------------------------*
 * This function retrieves the temperature information from the
 * spectrometer and returns this information via a temperature
 * structure.
 *
 * Input: None.
 *
 * Return value: pointer to a temperature struct with temperature
 *               data. Throws exception on failure.
 *----------------------------------------------------------------*/

struct temperature *
oriel_matrix_get_CCD_temp( void )
{
    unsigned char *readbuf;
    static struct temperature temp;


    fsc2_assert( oriel_matrix.has_temp_control );

    readbuf = oriel_matrix_communicate( CMD_GET_CCD_TEMPERATURE );

    temp.regulation   = readbuf[ 7 ];
    temp.set_point    = device_2_float( readbuf + 8 );
    temp.current_temp = device_2_float( readbuf + 12 );
    temp.therm_fault  = readbuf[ 16 ];
    temp.temp_lock    = readbuf[ 17 ];

    if ( temp.therm_fault )
    {
        print( FATAL, "Temperature Thermal Fault error.\n" );
        THROW( EXCEPTION );
    }

    return &temp;
}


/*--------------------------------------------------------------------*
 * This function sets the CCD temperature setpoint if temperature
 * regulation is supported.
 *
 * Input:
 *   double temp: The desired CCD temperature setpoint in degrees C.
 *
 * Return value: Returns -274.0 (below absolute zero) if there is an
 *               error else returns a CCD setpoint as a confirmation.
 *               On failure an exception gets thrown.
 *--------------------------------------------------------------------*/

double
oriel_matrix_set_CCD_temp( double temperature )
{
    struct temperature *temp;


    fsc2_assert( oriel_matrix.has_temp_control );

    oriel_matrix_communicate( CMD_SET_CCD_TEMPERATURE, 0x01,
                              temperature );

    temp = oriel_matrix_get_CCD_temp( );
    return temp->set_point;
}


/*---------------------------------------------------------*
 * Disabled CCD temperature regulation of the spectrometer
 *
 * Input: None
 *
 * Return value: none, throws an exception on failure
 *---------------------------------------------------------*/

void
oriel_matrix_disable_CCD_temp_regulation( void )
{
    oriel_matrix_communicate( CMD_SET_CCD_TEMPERATURE, 0x00, 0.0 );
}


/*----------------------------------------------------------------------*
 * This command set the current AFE offset and gain settings of
 * the spectrometer's camera.
 *
 * Inputs:
 *   unsignd short int offset: Specifies the current dark current
 *                             compensation.
 *                             Acceptable values: 0 - 1024.
 *   unsigned short int gain:  Specifies the current input sensitivity
 *                  to the full scale output of the CCD
 *                  Acceptable values: 0 - 63.
 *
 * Return value: none, function thrwos exception on failure
 *
 * ****NOTE**** I have yet to get this function to work properly with
 *              my spectrometer my guess is that not all spectrometers
 *              support this function. Unfortunatley there is no way to
 *              test if your spectrometer supports this function other
 *              than trying the funnction yourself and seeing if the
 *              function works without throwing an exception.
 *----------------------------------------------------------------------*/

void 
oriel_matrix_set_AFE_parameters( unsigned short offset,
                                 unsigned short gain )
{
    fsc2_assert( offset <= MAX_AFE_OFFSET );
    fsc2_assert( gain <= MAX_AFE_GAIN );

    oriel_matrix_communicate( CMD_SET_CCD_TEMEPRATURE_INFO, offset, gain );
}


/*----------------------------------------------------------------*
 * This function returns the current AFE offset and gain settings
 * of the spectrometer's camera.
 *
 * Input: None.
 *
 * Return value: an unsigned short int array of size 2 which
 *               contains the following inormation:
 *               index = 0  offset - Specifies the current dark
 *                          current. Possible values: 0 - 1024.
 *               index = 1  gain - specifies the current input
 *                          sensitivity to the full scale output
 *                          of the CCD. Possible values: 0 - 63.
 *               On failure an exception is thrown
 *----------------------------------------------------------------*/

unsigned short int *
oriel_matrix_get_AFE_parameters( void )
{
    unsigned char *readbuf;
    static unsigned short AFEPram[ 2 ];


    readbuf = oriel_matrix_communicate( CMD_GET_CCD_TEMEPRATURE_INFO );

    AFEPram[ 0 ] = device_2_ushort( readbuf + 9 );
    AFEPram[ 1 ] = device_2_ushort( readbuf + 11 );

    if (    AFEPram[ 0 ] > MAX_AFE_OFFSET
         || AFEPram[ 1 ] > MAX_AFE_GAIN )
    {
        print( FATAL, "Device returns invalid AFE parameters.\n" );
        THROW( EXCEPTION );
    }

    return AFEPram;
}


/*---------------------------------------------------------*
 * Function dealing with most of the comunication with the
 * device. Needs at least one argument, the command to be
 * send and then as many arguments as the command expect.
 * Returns a pointer to a static buffer with the reply or
 * throws an exception on failure to communicate with the
 * device.
 *---------------------------------------------------------*/

static unsigned char *
oriel_matrix_communicate( unsigned char cmd,
                          ... )
{
    unsigned char writebuf[ USB_WRITE_BUF_SIZE ];
    static unsigned char readbuf[ USB_READ_BUF_SIZE ];
    unsigned int len = 6;
    int rep = EP8;
    unsigned char uc;
    unsigned short int us;
    float f;
    va_list ap;
    const char *err = NULL;


    switch ( cmd )
    {
        case CMD_GET_CCD_TEMPERATURE :
            err = "Failed to get CCD temperature.\n";
            break;

        case CMD_SET_CCD_TEMPERATURE :
            len = 11;
            va_start( ap, cmd );
            uc = va_arg( ap, int );
            writebuf[ 6 ] = uc;      /* enable/disable temp. regulation */
            f = va_arg( ap, double );
            float_2_device( writebuf + 7, f );
            va_end( ap );
            err = "Failed to set CCD temperature.\n";
            break;

        case CMD_OPEN_CCD_SHUTTER :
            err = "Failed to open CCD shutter.\n";
            break;

        case CMD_CLOSE_CCD_SHUTTER :
            err = "Failed to close CCD shutter.\n";
            break;

        case CMD_GET_EXPOSURE_TIME :
            err = "Failed to read exposure time.\n";
            break;

        case CMD_SET_EXPOSURE_TIME :
            len = 10;
            va_start( ap, cmd );
            f = va_arg( ap, double );             /* temperature as float */
            va_end( ap );
            float_2_device( writebuf + 6, f );
            err = "Failed to set exposure time.\n";
            break;

        case CMD_START_EXPOSURE :
            len = 8;
            va_start( ap, cmd );
            uc = va_arg( ap, int );               /* shutter state */
            fsc2_assert( uc <= 0x02 );
            writebuf[ 6 ] = uc;
            uc = va_arg( ap, int );               /* exposure type */
            fsc2_assert( uc <= 0x03 );
            writebuf[ 7 ] = uc;
            va_end( ap );
            err = "Failed to start exposure.\n";
            break;

        case CMD_QUERY_EXPOSURE :
            err = "Failed to query exposure state.\n";
            break;

        case CMD_END_EXPOSURE :
            len = 7;
            va_start( ap, cmd );
            uc = va_arg( ap, int );               /* shutter state */
            writebuf[ 6 ] = uc;
            va_end( ap );
            err = "Failed to end exposure.\n";
            break;

        case CMD_GET_EXPOSURE :
            err = "Failed to get exposure.\n";
            rep = EP6;
            break;

        case CMD_GET_RECONSTRUCTION :
            len = 7;
            va_start( ap, cmd );
            uc = va_arg( ap, int );
            fsc2_assert( uc >= RECON_TYPE_LIGHT && uc <= RECON_TYPE_REFERENCE );
            writebuf[ 6 ] = uc;
            va_end( ap );
            err = "Failed to get reconstruction.\n";
            break;

        case CMD_SET_RECONSTRUCTION :
            len = 7;
            va_start( ap, cmd );
            uc = va_arg( ap, int );
            writebuf[ 6 ] = uc;
            va_end( ap );
            err = "Failed to set reconstruction.\n";
            break;

        case CMD_GET_LAST_ERROR :
            err = "Failed to get last error.\n";
            break;

        case CMD_GET_MODEL_NUMBER :
            err = "Failed to get model number.\n";
            break;

        case CMD_GET_SERIAL_NUMBER :
            err = "Failed to get serial number.\n";
            break;

        case CMD_GET_SPECTROMETER_INFO :
            err = "Failed to read device info.\n";
            break;

        case CMD_GET_CLOCK_RATE :
            err = "Failed to read clock rate.\n";
            break;

        case CMD_SET_CLOCK_RATE :
            len = 8;
            va_start( ap, cmd );
            us = va_arg( ap, int );
            fsc2_assert( us <= AD_CLOCK_FREQ_1000kHz );
            ushort_2_device( writebuf + 6, us );
            va_end( ap );
            err = "Failed to set clock rate.\n";
            break;

        case CMD_GET_PIXEL_MODE :
            err = "Failed to get pixel mode.\n";
            break;

        case CMD_SET_PIXEL_MODE :
            len = 8;
            va_start( ap, cmd );
            us = va_arg( ap, int );
            fsc2_assert( us <= PIXEL_MODE_LINE_BINNING );
            ushort_2_device( writebuf + 6, us );
            va_end( ap );
            err = "Failed to set pixel mode.\n";
            break;

        case CMD_RESET :
            err = "Failed to reset device.\n";
            break;

        case CMD_GET_CCD_TEMEPRATURE_INFO :
            err = "Failed to get AFE parameters.\n";
            break;

        case CMD_SET_CCD_TEMEPRATURE_INFO :
            len = 13;
            va_start( ap, cmd );
            us = va_arg( ap, int );                   /* offset */
            fsc2_assert( us <= MAX_AFE_OFFSET );
            ushort_2_device( writebuf + 9, us );
            us = va_arg( ap, int );                   /* gain */
            fsc2_assert( us <= MAX_AFE_GAIN );
            ushort_2_device( writebuf + 11, us );
            va_end( ap );
            err = "Failed to set AFE parameters.\n";
            break;

        default:
            fsc2_impossible( );
    }

    writebuf[ 0 ] = cmd;
    uint_2_device( writebuf + 1, len );
    writebuf[ 5 ] = SCHEME_NUMBER;

    raise_permissions( );

    if (    usb_bulk_write( oriel_matrix.udev, EP4, ( char * ) writebuf,
                            len, 0 ) != ( int ) len
         || usb_bulk_read( oriel_matrix.udev, rep, ( char * ) readbuf,
                           sizeof readbuf, 0) <= 0
         || readbuf[ 6 ] == 0x00 )
    {
        lower_permissions( );
        print( FATAL, err );
        THROW( EXCEPTION );
    }

    lower_permissions( );

    return readbuf;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

static unsigned short int
device_2_ushort( unsigned char * src )
{
    return ( src[ 1 ] << 8 ) | src[ 0 ];
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

static void
ushort_2_device( unsigned char * dest,
                 unsigned short  val )
{
    dest[ 0 ] = val && 0xFF;
    dest[ 1 ] = ( val >> 8 ) & 0xFF;
}


/*--------------------------------------------------------*
 * This requires to work that the machine this is running
 * is using 2s-complement
 *--------------------------------------------------------*/

static short int
device_2_short( unsigned char * src )
{
    unsigned short int val;

    if ( sizeof( short int ) < 2 )
    {
        print( FATAL, "Module won't work on this machine, sizeof(short int) "
               "is less than 2.\n" );
        THROW( EXCEPTION );
    }

    val =  ( src[ 1 ] << 8 ) | src[ 0 ];

    if ( sizeof( short int ) == 2 )
        return val;

    if ( src[ 1 ] & 0x80 )
        val |= ~ 0U ^ 0xFFFF;

    return val;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

static unsigned int
device_2_uint( unsigned char * src )
{
    return    ( src[ 3 ] << 24 ) | ( src[ 2 ] << 16 )
           |  ( src[ 1 ] <<  8 ) | src[ 0 ];
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

static void
uint_2_device( unsigned char * dest,
               unsigned int    val )
{
    dest[ 0 ] = val & 0xFF;
    dest[ 1 ] = ( val >>  8 ) & 0xFF;
    dest[ 2 ] = ( val >> 16 ) & 0xFF;
    dest[ 3 ] = ( val >> 24 ) & 0xFF;
}


/*-------------------------------------------------------------*
 * This needs to be changed if the machine this is running on
 * as a different float representation than the one the device
 * is using!
 *-------------------------------------------------------------*/

static float
device_2_float( unsigned char * src )
{
    float val;


    fsc2_assert( sizeof( val ) == 4 );

    memcpy( &val, src, sizeof val );
    return val;
}


/*-------------------------------------------------------------*
 * This needs to be changed if the machine this is running on
 * as a different float representation than the one the device
 * is using!
 *-------------------------------------------------------------*/

static void
float_2_device( unsigned char * dest,
                float           val )
{
    fsc2_assert( sizeof( val ) == 4 );

    memcpy( dest, &val, sizeof val );
}


/*---------------------------------------------------------*
 * Function is only used during development/debugging
 *---------------------------------------------------------*/

#if 0

static char *
oriel_matrix_get_model_number( void )
{
    unsigned char *readbuf;
    char * model_number;
    size_t len;


    readbuf = oriel_matrix_communicate( CMD_GET_MODEL_NUMBER );

    len = device_2_ushort( readbuf + 7 );
    model_number = T_malloc( len + 1 );
    model_number[ len ] = '\0';
    memcpy( model_number, readbuf + 9, len );

    return model_number;
}

#endif


/*---------------------------------------------------------*
 * Function is only used during development/debugging
 *---------------------------------------------------------*/

#if 0

static char *
oriel_matrix_get_serial_number( void )
{
    unsigned char *readbuf;
    char *serial_number;
    size_t len;


    readbuf = oriel_matrix_communicate( CMD_GET_SERIAL_NUMBER );
    len = device_2_ushort( readbuf + 7 );
    serial_number = T_malloc( len + 1 );
    serial_number[ len ] = '\0';
    memcpy( serial_number, readbuf + 9, len );

    return serial_number;
}

#endif


/*-----------------------------------------------------------*
 * This function simply prints out various information about
 * the spectrometer to the console
 *
 * Input: None
 *
 * Return value: none, on failure an exception gets thhrown
 *
 * Function is only used during development/debugging
 *-----------------------------------------------------------*/

#if 0

static void
oriel_matrix_print_info( void )
{  
    unsigned char *readbuf;
    unsigned short int major,
                       minor,
                       build;
    char *ptr;


    readbuf = oriel_matrix_communicate( CMD_GET_SPECTROMETER_INFO );

    fprintf( stderr, "\n");
    fprintf( stderr, "CCD pixel width: %u\n", device_2_ushort( readbuf + 7 ) );
    fprintf( stderr, "CCD pixel height: %u\n",
             device_2_ushort( readbuf + 9 ) );
    fprintf( stderr, "CCD bits per pixel: %u\n", readbuf[ 11 ] );
    fprintf( stderr, "CCD Type: %u\n", readbuf[ 12 ]);
    fprintf( stderr, "CCD Shutter support: %s\n",
             readbuf[ 13 ] ? "true" : "false");
    fprintf( stderr, "Minimum exposure time (s): %f\n",
             1.0e-3 * device_2_uint( readbuf + 14 ));
    fprintf( stderr, "Maximum exposure time (s): %f\n",
             1.0e-3 * device_2_uint( readbuf + 18 ) );
    fprintf( stderr, "Calibration support: %s\n",
             readbuf[ 22 ] ? "true" : "false" );
    fprintf( stderr, "Spectra reconstruction support: %s\n",
             readbuf[ 23 ] ? "true" : "false" );
    fprintf( stderr, "Spectra temperature regulation support: %s\n",
             readbuf[ 24 ] ? "true" : "false" );

    if ( readbuf[ 24 ] )
    {
        fprintf( stderr, "   Minimum CCD setpoint (C): %d\n",
                 device_2_short( readbuf + 25 ) );
        fprintf( stderr, "   Maximum CCD setpoint (C): %d\n",
                 device_2_short( readbuf + 27 ) );
    }

    fprintf( stderr, "Number of supported lasers: %u\n", readbuf[ 29 ] );
    fprintf( stderr, "Laser temperature regulation support: %s\n",
             readbuf[ 30 ] ? "true" : "false" );

    if ( readbuf[ 30 ] )
    {
        fprintf( stderr, "   Minimum laser setpoint: %i\n",
                 device_2_short( readbuf + 31 ) );
        fprintf( stderr, "   Maximum laser setpoint: %i\n",
                 device_2_short( readbuf + 33 ) );
    }

    fprintf( stderr, "Laser Power Regulation: %s\n",
             readbuf[ 35 ] ? "true" : "false" );

    if ( readbuf[ 35 ] )
    {
        fprintf( stderr, "   Minimum laser power setpoint: %d\n",
                 device_2_short( readbuf + 36 ) );
        fprintf( stderr, "   Maximum laser power setpoint: %d\n",
                 device_2_short( readbuf + 38 ) );
    }

    /* DSP firmware info */

    major = ( device_2_uint( readbuf + 40 ) >> 24 ) & 0xFF;
    minor = ( device_2_uint( readbuf + 40 ) >> 16 ) & 0xFF;
    build = device_2_uint( readbuf + 40 ) & 0x0000FFFF;
    fprintf( stderr, "DSP firmware version: %u.%u.%u\n", major, minor, build );

    /* FPGA firmware info */

    major = ( device_2_uint( readbuf + 44 ) >> 24 ) & 0xFF;
    minor = ( device_2_uint( readbuf + 44 ) >> 16 ) & 0xFF;
    build = device_2_uint( readbuf + 44 ) & 0x0000FFFF;
    fprintf( stderr, "FPGA firmware version: %u.%u.%u\n", major, minor, build );

    /* USB firmware info */

    major = ( device_2_uint( readbuf + 48 ) >> 24 ) & 0xFF;
    minor = ( device_2_uint( readbuf + 48 ) >> 16 ) & 0xFF;
    build = device_2_uint( readbuf + 48 ) & 0x0000FFFF;
    printf("USB firmware version: %u.%u.%u\n", major, minor, build);

    fprintf( stderr, "Number of general I/O lines: %d\n", readbuf[ 52 ] );

    switch ( readbuf[ 54 ] )
    {
        case AD_CLOCK_FREQ_NA :
            fprintf( stderr, "A/D clock frequency: N/A\n" );
            break;

        case AD_CLOCK_FREQ_DEFAULT :
            fprintf( stderr, "A/D clock frequency: default\n" );
            break;

        case AD_CLOCK_FREQ_150kHz :
            fprintf( stderr, "A/D clock frequency: 150KHz\n" );
            break;

        case AD_CLOCK_FREQ_300kHz :
            fprintf( stderr, "A/D clock frequency: 300KHz\n" );
            break;

        case AD_CLOCK_FREQ_500kHz :
            fprintf( stderr, "A/D clock frequency: 500KHz\n" );
            break;

        case AD_CLOCK_FREQ_1000kHz :
            fprintf( stderr, "A/D clock frequency: 1000KHz\n" );
            break;

        default:
            fprintf( stderr,"A/D clock frequency: Error\n" );
            break;
    }

    switch( readbuf[ 56 ] )
    {
        case PIXEL_MODE_NA :
            fprintf( stderr, "Pixel binning mode: N/A\n" );
            break;

        case PIXEL_MODE_FULL_WELL :
            fprintf( stderr, "Pixel binning mode: Full Well\n" );
            break;

        case PIXEL_MODE_DARK_CURRENT :
            fprintf( stderr, "Pixel binning mode: Dark Current\n" );
            break;

        case PIXEL_MODE_LINE_BINNING :
            fprintf( stderr, "Pixel binning mode: Line Binning\n" );
            break;

        default:
            fprintf( stderr, "Pixel binning mode: Error\n");
            break;
    }

    ptr = oriel_matrix_get_model_number( );
    fprintf( stderr, "Model Number: %s\n", ptr );
    T_free( ptr );

    ptr = oriel_matrix_get_serial_number( );
    fprintf( stderr, "Serial Number: %s\n", ptr );
    T_free( ptr );
 
    fprintf( stderr, "\n" );
}

#endif


/*---------------------------------------------------------------------*
 * This function prints out to the console and returns the last
 * known error that the spectrometer generated.
 *
 * Inputs: None
 * 
 * Return value: on failure an exception gets thrown, otherwise
 *               an unsigned integer array which contain the
 *               following error information
 *                  index = 0: Error Code
 *                          Possible values are:
 *                          0x0000 - No Errors
 *                          0x0001 - On-Chip Memory
 *                          0x0002 - On-Chip Digital I/O
 *                          0x0003 - On-Chip Interrupt Module
 *                          0x0004 - On-Chip Timer Module
 *                          0x0005 - On Chip I2C Module
 *                          0x0006 - On-Board ADC
 *                          0x0007 - On Board AFE
 *                          0x0008 - On Board EEPROM (DSP)
 *                          0x0009 - On-Board EEPROM (Cypress USB)
 *                          0x000A - On-Board Supply/Reference Voltage
 *                          0x000B - On-Board USB Controller (Cypress)
 *                          0x000C - USB Command Handler
 *                          0x000D - Non-Specific Source
 *                          0x000E - Calibration Module
 *                          0x000F - Timing FPGA
 *                  index = 1: Error Type
 *                          0x0000 - No Errors
 *                          0x0001 - Read/Write Failure
 *                          0x0002 - Invalid Address
 *                          0x0004 - Invalid Parameter
 *                          0x0008 - Invalid State
 *                          0x0010 - Allocation Failure
 *                          0x0020 - Limit Exceeded
 *                          0x0040 - Test/Diagnostic Failure
 *                          0x0080 - Non-Specific Failure
 *
 * Function is only used during development/debugging
 *---------------------------------------------------------------------*/

#if 0

static unsigned int *
oriel_matrix_get_last_error( void )
{
    unsigned char *readbuf;
    char error_code[ 80 ];
    char error_type[ 80 ];
    static unsigned int error[ 2 ];


    readbuf = oriel_matrix_communicate( CMD_GET_LAST_ERROR );

    switch ( readbuf[ 7 ] )
    {
        case 0x00:
            strcpy( error_code, "No Errors" );
            break;

        case 0x01:
            strcpy( error_code, "On-Chip Memory" );
            break;

        case 0x02:
            strcpy( error_code, "On-Chip Digital I/O" );
            break;

        case 0x03:
            strcpy( error_code, "On-Chip Interrupt Module" );
            break;

        case 0x04:
            strcpy( error_code, "On-Board Timer Module" );
            break;

        case 0x05:
            strcpy( error_code, "On-Chip I2C Module" );
            break;

        case 0x06:
            strcpy( error_code, "On-Board ADC" );
            break;

        case 0x07:
            strcpy( error_code, "On-Board AFE" );
            break;

        case 0x08:
            strcpy( error_code, "On-Board EEPROM (DSP)" );
            break;

        case 0x09:
            strcpy( error_code, "On-Board EEPROM (Cypress USB)" );
            break;

        case 0x0A:
            strcpy( error_code, "On-Board USB Controller (Cypress)" );
            break;

        case 0x0B:
            strcpy( error_code, "On-Board Supply/References Voltage" );
            break;

        case 0x0C:
            strcpy( error_code, "USB Command Handler" );
            break;

        case 0x0D:
            strcpy( error_code, "Non-Specific Source" );
            break;

        case 0x0E:
            strcpy( error_code, "Calibration Module" );
            break;

        case 0x0F:
            strcpy( error_code, "Timing FPGA" );
            break;

        default:
            strcpy( error_code, "Invalid Error Code" );
            break;
    }

    switch ( readbuf[ 9 ] )
    {
        case 0x00:
            strcpy( error_type, "No Errors" );
            break;

        case 0x01:
            strcpy( error_type, "Read/Write Failure" );
            break;

        case 0x02:
            strcpy( error_type, "Invalid Address" );
            break;

        case 0x04:
            strcpy( error_type, "Invalid Parameter" );
            break;

        case 0x08:
            strcpy( error_type, "Invalid State" );
            break;

        case 0x10:
            strcpy( error_type, "Allocation Failure" );
            break;

        case 0x20:
            strcpy( error_type, "Limit Exceeded" );
            break;

        case 0x40:
            strcpy( error_type, "Test/Diagnostic Failure" );
            break;

        case 0x80:
            strcpy( error_type, "Non-Specific Failure" );
            break;

        default:
            strcpy( error_type, "Invalid Error Type" );
            break;
    }

    fprintf( stderr, "Error Code: %s\nError Type: %s\n",
             error_code, error_type );

    error[ 0 ] = device_2_ushort( readbuf + 7 );
    error[ 1 ] = device_2_ushort( readbuf + 9 );

    return error;
}

#endif


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
