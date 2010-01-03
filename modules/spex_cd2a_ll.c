/*
 *  Copyright (C) 1999-2010 Jens Thoms Toerring
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


#define PARAMETER  0
#define COMMAND    1

#define STX   '\x02'
#define ETX   '\x03'
#define EOT   '\x04'
#define ACK   '\x06'
#define NAK   '\x15'
#define CAN   '\x18'


static void spex_cd2a_scan_end( void );

static void spex_cd2a_print( char * mess,
                             int    digits,
                             double val );

static size_t spex_cd2a_write( int          type,
                               const char * mess );

static void spex_cd2a_read_ack( void );

static char * spex_cd2a_read_mess( ssize_t to_be_read );

static void spex_cd2a_read_cmd_ack( const char * cmd );

static void spex_cd2a_read_set_pos_ack( void );

static void spex_cd2a_read_start_scan_ack( void );

static void spex_cd2a_read_scan_ack( void );

static void spex_cd2a_comm_fail( void );

static void spex_cd2a_wrong_data( void );

static ssize_t spex_cd2a_calc_pos_mess_len( void );

static void spex_cd2a_pos_mess_check( const char * bp );


static ssize_t spex_cd2a_pos_mess_len;
static bool spex_cd2a_do_print_message = UNSET;


/*--------------------------------------------------------------*
 * Function for initializing the monochromator to bring it into
 * a well-defined state.
 *--------------------------------------------------------------*/

void
spex_cd2a_init( void )
{
    /* First calculate the length of position messages, so we don't have to
       recalculate it all of the time. */

    spex_cd2a_pos_mess_len = spex_cd2a_calc_pos_mess_len( );

    /* Now check if the device is prepared to talk with us by sending it the
       command to use burst scans - that's the only type of scans supported
       anyway (with bursts induced by software triggers). */

    spex_cd2a_do_print_message = UNSET;

#ifndef SPEX_CD2A_TEST
    while ( 1 )
    {
        TRY
        {
            fsc2_tcflush( spex_cd2a.sn, TCIOFLUSH );
            spex_cd2a_write( PARAMETER, "TYB" );
            TRY_SUCCESS;
        }
        CATCH( EXCEPTION )
        {
            if ( 1 != show_choices( "Please press the \"REMOTE\" button at\n"
                                    "the console to allow computer control\n"
                                    "of the monochromator.",
                                    2, "Abort", "Done", NULL, 1 ) )
                continue;
            SPEX_CD2A_THROW( EXCEPTION );
        }
        OTHERWISE
            SPEX_CD2A_RETHROW( );

        break;
    }
#endif

    spex_cd2a_do_print_message = SET;

    /* Set the laser line position if the monochromator is wavenumber driven
       (if not set the laser line gets switched off automatically to make
       the device accept wavenumbers in absolute units). */

    if ( spex_cd2a.mode & WN_MODES )
        spex_cd2a_set_laser_line( );

    /* Set the end of scans to the upper wavelength or lower wavenumber limit
       and the number of scan repetitions to 1. */

    spex_cd2a_scan_end( );
    spex_cd2a_write( PARAMETER, "NS1" );

    /* If a wavelength was set in the PREPARATIONS section set it now. */

    if ( spex_cd2a.is_wavelength )
        spex_cd2a_set_wavelength( );

    /* If there was a scan setup send the values to the device now and, if
       no wavelength or -number was set, set it to the value of the start
       position of the scan. */

    if ( spex_cd2a.scan_is_init )
    {
        spex_cd2a_scan_start( );
        spex_cd2a_scan_step( );

        if ( ! spex_cd2a.is_wavelength )
        {
            spex_cd2a_set_wavelength( );
            spex_cd2a.is_wavelength = SET;
        }
    }
}


/*--------------------------------------------------*
 * Function for setting a new wavelength or -number
 *--------------------------------------------------*/

void
spex_cd2a_set_wavelength( void )
{
    char mess[ 11 ] = "SE";


    /* If we're in scan mode we need to stop the currently running scan
       before we can set a new position */

    if ( spex_cd2a.in_scan )
        spex_cd2a_halt( );

    if ( FSC2_MODE != EXPERIMENT )
        return;

    /* Set the wavenumber or -length, depending on monochromator type */

    spex_cd2a_print( mess + 2, 8, spex_cd2a_wl2mu( spex_cd2a.wavelength ) );

    spex_cd2a_write( PARAMETER, mess );

    /* ...and ask the monochromator to move to it */

    spex_cd2a_write( COMMAND, "P" );
}


/*------------------------------*
 * Function for stopping a scan
 *------------------------------*/

void
spex_cd2a_halt( void )
{
    if ( FSC2_MODE == EXPERIMENT )
        spex_cd2a_write( COMMAND, "H" );
    spex_cd2a.in_scan = UNSET;
}


/*------------------------------------------------*
 * Function for starting a (triggered burst) scan
 *------------------------------------------------*/

void
spex_cd2a_start_scan( void )
{
    fsc2_assert( spex_cd2a.scan_is_init );

    /* If we're already scanning stop the current scan */

    if ( spex_cd2a.in_scan )
        spex_cd2a_halt( );

    if ( FSC2_MODE == EXPERIMENT )
        spex_cd2a_write( COMMAND, "T" );

    spex_cd2a.in_scan = SET;
    spex_cd2a.wavelength = spex_cd2a.scan_start;
}


/*--------------------------------------------------------*
 * Function sends a trigger (only possible during a scan)
 *--------------------------------------------------------*/

void
spex_cd2a_trigger( void )
{
    fsc2_assert( spex_cd2a.in_scan );

    if ( FSC2_MODE == EXPERIMENT )
        spex_cd2a_write( COMMAND, "E" );

    if ( spex_cd2a.mode == WL )
        spex_cd2a.wavelength += spex_cd2a.scan_step;
    else
        spex_cd2a.wavelength =
                       spex_cd2a_wn2wl( spex_cd2a_wl2wn( spex_cd2a.wavelength )
                                        - spex_cd2a.scan_step );
}


/*-------------------------------------------------------------------*
 * Function for setting the position of the laser line. This is only
 * possible when the monochromator is wavenumber driven. It also
 * switches the way positions are treated by the device: if no laser
 * line position is set positions are taken to be absolute wave-
 * numbers. But when the laser line is set positions are taken to be
 * relative to the laser line (i.e. position of the laser line minus
 * the absolute position). The only exception is when setting the
 * laser line position itself: the value passed to the device is
 * always taken to be a position in  absolute wavenumbers.
 * The laser line position can also be switched off (thus reverting
 * to absolute positions) by setting the laser line position to 0.
 *-------------------------------------------------------------------*/

void
spex_cd2a_set_laser_line( void )
{
    char mess[ 11 ] = "LL";


    fsc2_assert( spex_cd2a.mode & WN_MODES );

    if ( FSC2_MODE != EXPERIMENT )
        return;

    fsc2_assert( spex_cd2a.mode & WN_MODES );

    /* Stop a running scan */

    if ( spex_cd2a.in_scan )
        spex_cd2a_halt( );

    spex_cd2a_print( mess + 2, 8, spex_cd2a.laser_line );
    spex_cd2a_write( PARAMETER, mess );
}


/*-------------------------------------------------------*
 * Function for setting the shutter limits of the device
 *-------------------------------------------------------*/

void
spex_cd2a_set_shutter_limits( void )
{
    char mess[ 11 ] = "SL";


    fsc2_assert( spex_cd2a.has_shutter );

    /* Halt a running scan */

    if ( spex_cd2a.in_scan )
        spex_cd2a_halt( );

    if ( FSC2_MODE != EXPERIMENT )
        return;

    /* Set the lower shutter limit */

    spex_cd2a_print( mess + 2, 8, spex_cd2a_wl2mu( spex_cd2a.lower_limit ) );
    spex_cd2a_write( PARAMETER, mess );

    /* Set the upper shutter limit */

    strcpy( mess, "SH" );
    spex_cd2a_print( mess + 2, 8, spex_cd2a_wl2mu( spex_cd2a.upper_limit ) );
    spex_cd2a_write( PARAMETER, mess );
}


/*--------------------------------------------------*
 * Function for setting the start position of scans
 *--------------------------------------------------*/

void
spex_cd2a_scan_start( void )
{
    char mess[ 11 ] = "ST";


    /* If a scan is already running halt it first */

    if ( spex_cd2a.in_scan )
        spex_cd2a_halt( );

    if ( FSC2_MODE != EXPERIMENT )
        return;

    /* Set start wavenumber or -length, depending on monochromator type */

    spex_cd2a_print( mess + 2, 8, spex_cd2a_wl2mu( spex_cd2a.scan_start ) );
    spex_cd2a_write( PARAMETER, mess );
}


/*---------------------------------------------------------------------*
 * Function for setting the end point of scans to the upper wavelength
 * and lower wavenumber limit (since we never know the end point of a
 * scan in advance the end point is set to the maximum wavelength or
 * minimum wavenumber).
 *---------------------------------------------------------------------*/

static void
spex_cd2a_scan_end( void )
{
    char mess[ 11 ] = "EN";

    spex_cd2a_print( mess + 2, 8, spex_cd2a_wl2mu( spex_cd2a.upper_limit ) );
    spex_cd2a_write( PARAMETER, mess );
}


/*---------------------------------------------*
 * Function for setting the step size of scans
 *---------------------------------------------*/

void
spex_cd2a_scan_step( void )
{
    char mess[ 9 ] = "BI";


    if ( FSC2_MODE != EXPERIMENT )
        return;

    if ( spex_cd2a.mode & WN_MODES )
        spex_cd2a_print( mess + 2, 6, spex_cd2a.scan_step );
    else
        spex_cd2a_print( mess + 2, 6, ( UNITS == NANOMETER ? 1.0e9 : 1.0e10 )
                                      * spex_cd2a.scan_step );

    spex_cd2a_write( PARAMETER, mess );
}


/*-----------------------------------------------------------------------*
 * Function assembles a numeric string in the format the device expects.
 * The string is never going to be longer than 'digits'.
 *-----------------------------------------------------------------------*/

static void
spex_cd2a_print( char * mess,
                 int    digits,
                 double val )
{
    int pre_digits, after_digits;
    char *buf;


    fsc2_assert( digits > 0 );

    if ( log10( fabs( fabs( val  ) ) ) < 0 )
    {
        pre_digits = 1;
        if ( val < 0.0 )
            pre_digits += 1;
        after_digits = digits - pre_digits - 1;
    }
    else
    {
        buf = get_string( "%f", val );
        for ( pre_digits = 0; pre_digits < digits - 1; pre_digits++ )
            if ( buf[ pre_digits ] == '.' )
                break;
        T_free( buf );
        after_digits = digits - pre_digits - 1;
        fsc2_assert( after_digits >= 0 );
    }

    if ( after_digits == 0 )
        sprintf( mess, "%*ld", digits, lrnd( val ) );
    else
    {
        val = lrnd( val * pow( 10, after_digits ) ) / pow( 10, after_digits );
        sprintf( mess, "%*.*f", digits, after_digits, val );
    }
}


/*-----------------------------------------------------*
 * Function for opening the device file for the device
 * and setting up the communication parameters.
 *-----------------------------------------------------*/

void
spex_cd2a_open( void )
{
#ifndef SPEX_CD2A_TEST

    /* We need exclusive access to the serial port and we also need non-
       blocking mode to avoid hanging indefinitely if the other side does not
       react. O_NOCTTY is set because the serial port should not become the
       controlling terminal, otherwise line noise read as a CTRL-C might kill
       the program. */

    if ( ( spex_cd2a.tio = fsc2_serial_open( spex_cd2a.sn,
                          O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) ) == NULL )
    {
        print( FATAL, "Can't open device file for monochromator.\n" );
        SPEX_CD2A_THROW( EXCEPTION );
    }

    spex_cd2a.tio->c_cflag = 0;

    switch ( PARITY_TYPE )
    {
        case NO_PARITY :
            break;

        case ODD_PARITY :
            spex_cd2a.tio->c_cflag |= PARENB | PARODD;
            break;

        case EVEN_PARITY :
            spex_cd2a.tio->c_cflag |= PARENB;
            break;

        default :
            fsc2_serial_close( spex_cd2a.sn );
            print( FATAL, "Invalid setting for parity bit in "
                   "configuration file for the device.\n" );
            SPEX_CD2A_THROW( EXCEPTION );
    }

    switch ( NUMBER_OF_STOP_BITS )
    {
        case 1 :
            break;

        case 2 :
            spex_cd2a.tio->c_cflag |= CSTOPB;
            break;

        default :
            fsc2_serial_close( spex_cd2a.sn );
            print( FATAL, "Invalid setting for number of stop bits in "
                   "configuration file for the device.\n" );
            SPEX_CD2A_THROW( EXCEPTION );
    }

    switch ( NUMBER_OF_BITS_PER_CHARACTER )
    {
        case 5 :
            spex_cd2a.tio->c_cflag |= CS5;
            break;

        case 6 :
            spex_cd2a.tio->c_cflag |= CS6;
            break;

        case 7 :
            spex_cd2a.tio->c_cflag |= CS7;
            break;

        case 8 :
            spex_cd2a.tio->c_cflag |= CS8;
            break;

        default :
            fsc2_serial_close( spex_cd2a.sn );
            print( FATAL, "Invalid setting for number of bits per "
                   "in character configuration file for the device.\n" );
            SPEX_CD2A_THROW( EXCEPTION );
    }

    spex_cd2a.tio->c_cflag |= CLOCAL | CREAD;
    spex_cd2a.tio->c_iflag = IGNBRK;
    spex_cd2a.tio->c_oflag = 0;
    spex_cd2a.tio->c_lflag = 0;

    cfsetispeed( spex_cd2a.tio, SERIAL_BAUDRATE );
    cfsetospeed( spex_cd2a.tio, SERIAL_BAUDRATE );

    /* We can't use canonical mode here... */

    spex_cd2a.tio->c_lflag = 0;

    fsc2_tcflush( spex_cd2a.sn, TCIOFLUSH );
    fsc2_tcsetattr( spex_cd2a.sn, TCSANOW, spex_cd2a.tio );

#endif

    spex_cd2a.is_open = SET;
}


/*-----------------------------------------------------------------------*
 * Function for sending a message to the device. It also adds everything
 * required by the communication protocol (i.e. <STX>, <CAN>, <ETX>, the
 * checksum and <CR>).
 *-----------------------------------------------------------------------*/

static size_t
spex_cd2a_write( int          type,
                 const char * mess )
{
    unsigned char *tmx;
    unsigned long cs = 0;
    size_t len;
    size_t i;
    ssize_t written;


#ifdef SPEX_CD2A_TEST
    fprintf( stderr, "%s\n", mess );
    return 0;
#endif

    len = strlen( mess );
    tmx = T_malloc( len + 6 );

    tmx[ 0 ] = type == PARAMETER ? STX : CAN;
    len++;
    strcpy( ( char * ) ( tmx + 1 ), mess );
    tmx[ len++ ] = ETX;

    if ( spex_cd2a.use_checksum )
    {
        for ( i = 0; i < len; i++ )
            cs += tmx[ i ];
        sprintf( ( char * ) ( tmx + len ), "%02lx", cs & 0xFF );
        len += 2;
    }

    tmx[ len++ ] = '\r';

    /* The device don't seem to accept commands unless there's a short
       delay since the last transmission... */

    if ( type == COMMAND )
        fsc2_usleep( 10000, UNSET );

    if ( ( written = fsc2_serial_write( spex_cd2a.sn, tmx, len, 0, UNSET ) )
         <= 0 )
    {
        T_free( tmx );
        spex_cd2a_comm_fail( );
    }

    T_free( tmx );

    spex_cd2a_read_ack( );
    if ( type != PARAMETER )
        spex_cd2a_read_cmd_ack( mess );

    return ( size_t ) written;
}


/*-------------------------------------------------------------*
 * Function for reading the acknowledgment send by the device,
 * consisting of <ACK> and <CAN> in case of success, <ACK> and
 * <BEL> in case of command errors and a single <NAK> in case
 * of communication problems.
 *-------------------------------------------------------------*/

static void
spex_cd2a_read_ack( void )
{
    char buf[ 8 ];
    ssize_t received, len, count;


    /* Skip initial <CAN> characters, the device sometimes sends them without
       good reasons (at least in contrast to what's written in the manual) */

    do {
        if ( ( received = fsc2_serial_read( spex_cd2a.sn, buf, 1, NULL,
                                            1000000, UNSET ) ) <= 0 )
            spex_cd2a_comm_fail( );
    } while ( *buf == CAN );

    if (    *buf != NAK
         && ( received = fsc2_serial_read( spex_cd2a.sn, buf + 1, 1, NULL,
                                           1000000, UNSET ) ) <= 0 )
        spex_cd2a_comm_fail( );

    /* A <NAK> character means that there are communication problems */

    if ( *buf == NAK )
    {
        print( FATAL, "Communication problem with device.\n" );
        SPEX_CD2A_THROW( EXCEPTION );
    }

    /* If the device sends an <ACK><CAN> sequence everything is fine */

    if ( buf[ 0 ] == ACK && buf[ 1 ] == CAN )
        return;

    /* A <BEL> character after the <ACK> means something went wrong and we're
       going to get the error code in the next bytes. A complete error message
       consists of two (printable) ASCII characters for the error code, an
       <EOT> character, optionally two checksum bytes, a <CR> and an optional
       <LF>. */

    if ( buf[ 0 ] == ACK && buf[ 1 ] == '\a' )
    {
        count = 0;
        len = 4;
        if ( spex_cd2a.use_checksum )
            len += 2;
        if ( spex_cd2a.sends_lf )
            len++;

        while ( len > 0 )
        {
            if ( ( received = fsc2_serial_read( spex_cd2a.sn, buf + count + 2,
                                                len - count, NULL,
                                                1000000, UNSET ) )
                 <= 0 )
                spex_cd2a_comm_fail( );

            count += received;
            len -= received;
        }

        if ( buf[ 4 ] != EOT )
            spex_cd2a_comm_fail( );

        buf[ 4 ] = '\0';
        if ( spex_cd2a_do_print_message )
            print( FATAL, "Failure to execute command, error code: \"%s\".\n",
               buf + 2 );
        SPEX_CD2A_THROW( EXCEPTION );
    }

    /* If none of the above was received things went really wrong... */

    spex_cd2a_wrong_data( );
}


/*----------------------------------------------------*
 * Function for reading in a message sent by the CD2A
 *----------------------------------------------------*/

static char *
spex_cd2a_read_mess( ssize_t to_be_read )
{
    static char buf[ 20 ];
    ssize_t already_read = 0;
    ssize_t old_already_read, i;
    char *bp = buf + to_be_read - 1;


    while ( already_read < to_be_read )
    {
        old_already_read = already_read;

        if ( ( already_read +=
               				fsc2_serial_read( spex_cd2a.sn, buf + already_read,
                                              to_be_read - already_read, NULL,
                                              1000000, UNSET ) ) < 0 )
            spex_cd2a_comm_fail( );

        /* Throw away <CAN> characters, the device sends them sometimes in the
           middle of a message (contrary to what's written in the manual. But
           if the device sends a checksum we must be careful because the
           checksum could contain a value equivalent to <CAN>... */

        if ( ! spex_cd2a.use_checksum )
        {
            for ( i = old_already_read; i < already_read; i++ )
                if ( buf[ i ] == CAN )
                    memmove( buf + i, buf + i + 1, --already_read - i );
        }
        else
        {
            for ( i = old_already_read;
                  i < already_read
                  && i < to_be_read - ( spex_cd2a.sends_lf ? 4 : 3 );
                  i++ )
                if ( buf[ i ] == CAN )
                    memmove( buf + i, buf + i + 1, --already_read - i );

            for ( i = to_be_read - ( spex_cd2a.sends_lf ? 2 : 1 );
                  i < already_read; i++ )
                if ( buf[ i ] == CAN )
                    memmove( buf + i, buf + i + 1, --already_read - i );
        }
    }

    /* In STANDARD data format the first byte has to be a <STX> character */

    if ( spex_cd2a.data_format == STANDARD && buf[ 0 ] != STX )
        spex_cd2a_wrong_data( );

    /* Last character(s) of message must be <CR> or <CR><LR> */

    if ( ( spex_cd2a.sends_lf && *bp-- != '\n' ) || *bp-- != '\r' )
        spex_cd2a_wrong_data( );

    /* Finally check end of message character which (in standard output mode)
       is <ETX> */

    if ( spex_cd2a.data_format == STANDARD )
    {
        if ( spex_cd2a.use_checksum )
            bp -= 2;

        if ( *bp != ETX )
            spex_cd2a_wrong_data( );
    }

    return spex_cd2a.data_format == STANDARD ? buf + 1 : buf;
}


/*-------------------------------------------------------------*
 * Function for reading the acknowledgment send by the device
 * when it received a command. Most commands not only send an
 * ACK/CAN sequence but also make the device transmit position
 * information until the command is executed. This function
 * looks for the sequence to be expected for a command,
 * throwing an exception if it isn't coming from the device.
 *-------------------------------------------------------------*/

static void
spex_cd2a_read_cmd_ack( const char * cmd )
{
    switch ( *cmd )
    {
        case 'P' :                /* "GO TO SET POSITION" command */
            spex_cd2a_read_set_pos_ack( );
            break;

        case 'T' :                /* "ENABLE TRIGGER SCAN" command */
            spex_cd2a_read_start_scan_ack( );
            break;

        case 'E' :                /* "START TRIGGER SCAN" command */
            spex_cd2a_read_scan_ack( );
            break;

        case 'H' :                /* "HALT" command */
            break;

        default :                 /* no other commands are used */
            SPEX_CD2A_IMPOSSIBLE( );
    }
}


/*---------------------------------------------------------------------*
 * Function for handling of messages received after a position command
 * ("P") has been send to the device.
 *---------------------------------------------------------------------*/

static void
spex_cd2a_read_set_pos_ack( void )
{
    char *bp;


    /* Repeatedly read in the current position until the final position has
       been reached. */

    while ( 1 )
    {
        bp = spex_cd2a_read_mess( spex_cd2a_pos_mess_len );

        switch ( *bp++ )
        {
            case '*' :          /* final position reached ? */
                spex_cd2a_pos_mess_check( bp );
                return;

            case 'P' :          /* still moving to final position ? */
                spex_cd2a_pos_mess_check( bp );
                break;

            default :
                spex_cd2a_wrong_data( );
        }
    }
}


/*-----------------------------------------------------------------------*
 * Function for handling of messages received after a start trigger scan
 * command ("T") has been send to the device.
 *-----------------------------------------------------------------------*/

static void
spex_cd2a_read_start_scan_ack( void )
{
    char *bp;


    /* Repeatedly read in the current position until the start position for
       the scan is reached. */

    while ( 1 )
    {
        bp = spex_cd2a_read_mess( spex_cd2a_pos_mess_len );

        switch ( *bp++ )
        {
            case 'S' :          /* final position reached ? */
                spex_cd2a_pos_mess_check( bp );
                return;

            case 'P' :          /* still moving to final position ? */
                spex_cd2a_pos_mess_check( bp );
                break;

            default :
                spex_cd2a_wrong_data( );
        }
    }
}


/*--------------------------------------------------------------------*
 * Function for handling of messages received after a trigger command
 * ("E") during a burst scan has been send to the device.
 *--------------------------------------------------------------------*/

static void
spex_cd2a_read_scan_ack( void )
{
    char *bp;


    /* Repeatedly read in the position until the burst movement is complete */

    while ( 1 )
    {
        bp = spex_cd2a_read_mess( spex_cd2a_pos_mess_len );

        switch ( *bp++ )
        {
            case 'B' :          /* final position reached ? */
                spex_cd2a_pos_mess_check( bp );
                return;

            case 'P' :          /* still moving to final position ? */
                spex_cd2a_pos_mess_check( bp );
                break;

            default :
                spex_cd2a_wrong_data( );
        }
    }
}


/*-------------------------------------------------------------------*
 * Function that gets called to close the device file for the device
 *-------------------------------------------------------------------*/

void
spex_cd2a_close( void )
{
    if ( spex_cd2a.is_open )
        fsc2_serial_close( spex_cd2a.sn );
    spex_cd2a.is_open = UNSET;
}


/*-------------------------------------------------------*
 * Function to handle situations where the communication
 * with the device failed completely.
 *-------------------------------------------------------*/

static void
spex_cd2a_comm_fail( void )
{
    if ( spex_cd2a_do_print_message )
        print( FATAL, "Can't access the monochromator.\n" );
    SPEX_CD2A_THROW( EXCEPTION );
}


/*--------------------------------------------------*
 * Function for situations where the device reacted
 * but send a message it wasn't supposed to send.
 *--------------------------------------------------*/

static void
spex_cd2a_wrong_data( void )
{
    print( FATAL, "Device send unexpected data.\n" );
    SPEX_CD2A_THROW( EXCEPTION );
}


/*-------------------------------------------------------------------------*
 * Function calculates the length of position messages send by the device.
 * The message consists at least of a status char, a char indicating the
 * unit, an eighth byte long floating point number in ASCII format and a
 * carriage return. When STANDARD data format is used the message starts
 * with a STX char and an ETX char is send directly after tne number. If
 * checksums are transmitted two additional bytes are send directly before
 * carriage return. And if the device is set up to send a linefeed this is
 * send at the end of the string.
 *-------------------------------------------------------------------------*/

static ssize_t
spex_cd2a_calc_pos_mess_len( void )
{
    ssize_t len = 11;


    if ( spex_cd2a.data_format == STANDARD )
        len += 2;
    if ( spex_cd2a.use_checksum )
        len += 2;
    if ( spex_cd2a.sends_lf )
        len++;

    return len;
}


/*------------------------------------------------------------------------*
 * Reads in data send from the device with information about its position
 *------------------------------------------------------------------------*/

static void
spex_cd2a_pos_mess_check( const char * bp )
{
    char *ep;
    const char *eu = bp;
    double val;


    /* When the device is wavenumber driven it sends either a 'W' (when
       no laser line has been set) or a 'D' (for delta wavelength),
       otherwise either a 'N' or 'A', depending if it's set up to use
       nanometer or Angstrom units */

    if (    ( spex_cd2a.mode == WN_ABS && *bp == 'W' )
         || ( spex_cd2a.mode == WN_REL && *bp == 'D' )
         || ( spex_cd2a.mode == WL
              && (    ( spex_cd2a.units == NANOMETER && *bp == 'N' )
                   || ( spex_cd2a.units == ANGSTROEM && *bp == 'A' ) ) ) )
        bp++;
    else
        spex_cd2a_wrong_data( );

    /* Now follows an 8-byte long numeric field - for negative numbers
       we have a minus sign which is possibly followed by spaces before
       the number starts. */

    if ( *bp == '-' )
        bp++;

    errno = 0;
    val = strtod( bp, &ep );
    if ( errno || ep != eu + 9 )
        spex_cd2a_wrong_data( );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
