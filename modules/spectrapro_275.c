/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring/Ivo Alxneit
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "fsc2_module.h"
#include "serial.h"


/* Include configuration information for the device */

#include "spectrapro_275.conf"


/*--------------------------------*/
/* global variables of the module */
/*--------------------------------*/


/* MAGIC_WL / grooves_per_mm defines the maximum wavelength [m] allowed,
   some safety margin included */

#define MAGIC_WL 1.65e-3

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* --------------------------------- */
/* Declaration of exported functions */
/* --------------------------------- */

int spectrapro_275_init_hook(       void );
int spectrapro_275_test_hook(       void );
int spectrapro_275_exp_hook(        void );
int spectrapro_275_end_of_exp_hook( void );

Var_T * monochromator_name(             Var_T * /* v */ );
Var_T * monochromator_grating(          Var_T * /* v */ );
Var_T * monochromator_wavelength(       Var_T * /* v */ );


/* ------------------------------------------- */
/* Structure to keep track of current settimgs */
/* ------------------------------------------- */

struct SPECTRAPRO_275 {
    int sn;
    bool is_needed;         /* is the monochromator needed at all? */
    bool is_open;           /* is the device file open ? */
    struct termios *tio;    /* serial port terminal interface structure */
    double wavelength;      /* current wavelength [m]*/
    bool is_wavelength;     /* wavelength has been set */
    long current_gn;        /* current grating number, range 1-3 */
    double max_wl;          /* maximum wavelength [m] of current grating */
};

static struct SPECTRAPRO_275 spectrapro_275, spectrapro_275_stored;


/* -------------------------- */
/* Local, low level functions */
/* -------------------------- */

static double spectrapro_275_get_wavelength( void );
static void spectrapro_275_set_wavelength( double wavelength );
static long spectrapro_275_get_grating( void );
static void spectrapro_275_set_grating( long gn );
static void spectrapro_275_store_max_wl( void );


/* ---------------------- */
/* low level IO functions */
/* ---------------------- */

enum {
       SERIAL_INIT,
       SERIAL_EXIT,
       SERIAL_READ,
       SERIAL_WRITE
};

#define SPECTRAPRO_275_WAIT  100000   /* 100 ms */

static void spectrapro_275_open( void );
static void spectrapro_275_close( void );
static void spectrapro_275_comm_fail( void );
static bool spectrapro_275_comm( int type,
								 ... );
static bool spectrapro_275_read( char   * buf,
								 size_t * len );
static char *spectrapro_275_talk( const char * buf,
								  size_t       len );
static void spectrapro_275_send( const char * buf );



/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
spectrapro_275_init_hook( void )
{
    /* Claim the serial port (throws exception on failure) */

    spectrapro_275.sn = fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

    spectrapro_275.is_needed = SET;
    spectrapro_275.is_open = UNSET;

    /* Initialize structure with reasonable values used in TEST.
       in PREPARATION the wavelength can be set in this structure,
       but not yet transmitted to the monochromator. in this
       case it will be marked as SET und the data will be tranferred
       in the 'exp_hook'. */

    spectrapro_275.wavelength = 0.0;
    spectrapro_275.is_wavelength = UNSET;

    spectrapro_275.current_gn = 1;
    spectrapro_275.max_wl = 1.0e-5;    /* 10 um */

    return 1;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
spectrapro_275_test_hook( void )
{
    spectrapro_275_stored = spectrapro_275;
    return 1;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
spectrapro_275_exp_hook( void )
{
    spectrapro_275 = spectrapro_275_stored;

    if ( ! spectrapro_275.is_needed )
        return 1;

    spectrapro_275_open( );

    /* If a wavelength was set during the PREPARATIONS section set it now.
       otherwise set the current value in the structure */

    if ( spectrapro_275.is_wavelength )
        spectrapro_275_set_wavelength( spectrapro_275.wavelength );
    else
        spectrapro_275.wavelength = spectrapro_275_get_wavelength( );

    /* Query currently selected grating and set its maximum wavelength
       allowed */

    spectrapro_275.current_gn = spectrapro_275_get_grating( );
    spectrapro_275_store_max_wl( );

    return 1;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
spectrapro_275_end_of_exp_hook( void )
{
    if ( ! spectrapro_275.is_needed || ! spectrapro_275.is_open )
        return 1;

    spectrapro_275_close( );

    spectrapro_275.is_wavelength = UNSET;

    return 1;
}


/*----------------------------------------------*
 * Returns a string with the name of the device
 *----------------------------------------------*/

Var_T *
monochromator_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*------------------------------------------------------------*
 * Function for setting or quering the currently used grating
 *------------------------------------------------------------*/

Var_T *
monochromator_grating( Var_T * v )
{
    long gn;


    if ( v == 0 )
        return vars_push( INT_VAR, spectrapro_275.current_gn );

    gn = get_strict_long( v, "grating number" );

    if ( gn < 1 || gn > MAX_GRATINGS )
    {
        if ( FSC2_MODE == TEST )
        {
            print( FATAL, "Invalid grating number, valid range is 1 to %ld.\n",
                   MAX_GRATINGS );
            THROW( EXCEPTION );
        }

        print( SEVERE,  "Invalid grating number, keeping grating #%ld\n",
               spectrapro_275.current_gn );
        return vars_push( INT_VAR, spectrapro_275.current_gn );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
    {
        spectrapro_275_set_grating( gn );
        spectrapro_275.current_gn = gn;
        spectrapro_275_store_max_wl( );
    }
    return vars_push( INT_VAR, gn );
}


/*-----------------------------------------------------------------*
 * Function either returns the currently set wavelength (if called
 * with no arguments) or moves the grating to a new center wave-
 * length as specified (in meters) by the only argument.
 *-----------------------------------------------------------------*/

Var_T *
monochromator_wavelength( Var_T * v )
{
    double wl;


    if ( v == NULL )
    {
        if ( FSC2_MODE == EXPERIMENT )
            spectrapro_275.wavelength = spectrapro_275_get_wavelength( );
        return vars_push( FLOAT_VAR, spectrapro_275.wavelength );
    }

    wl = get_double( v, "wavelength" );

    if ( wl < 0.0 )
    {
        if ( FSC2_MODE == TEST )
        {
            print( FATAL, "Invalid negative wavelength.\n" );
            THROW( EXCEPTION );
        }

        print( SEVERE, "Invalid negative wavelength, using 0 nm instead.\n" );
        wl = 0.0;
    }

    /* Check that the wavelength isn't too large */

    if ( wl > spectrapro_275.max_wl )
    {
        if ( FSC2_MODE == TEST )
        {
            print( FATAL, "Wavelength of %.2f nm is too large, "
	           "maximum wavelength is %.2f nm.\n",
		   1.0e9 * wl, 1.0e9 * spectrapro_275.max_wl );
            THROW( EXCEPTION );
        }

        print( SEVERE, "Wavelength of %.2f nm is too large, "
	       "using %.2f nm instead.\n",
	       1.0e9 * wl, 1.0e9 * spectrapro_275.max_wl );
        wl = spectrapro_275.max_wl;
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        spectrapro_275_set_wavelength( wl );

    if ( FSC2_MODE == PREPARATION )  /* only store in structure and mark SET */
	{
        spectrapro_275.wavelength = wl;
        spectrapro_275.is_wavelength = SET;
    }

    return vars_push( FLOAT_VAR, spectrapro_275.wavelength );
}


/*-------------------------------------------------------*
 * Function asks the monochromator for the wavelength it
 * is currently set to.
 *-------------------------------------------------------*/

static double
spectrapro_275_get_wavelength( void )
{
    char *reply;
    double wl;


    reply = spectrapro_275_talk( "?NM", 100 );
    wl =  T_atod( reply );
    T_free( reply );
    return 1.0e-9 * wl;
}


/*------------------------------------------------------*
 * Function sets the monochromator to a new wavelength.
 *------------------------------------------------------*/

static void
spectrapro_275_set_wavelength( double wavelength )
{
    char *buf;


    fsc2_assert( wavelength >= 0.0 || wavelength <= spectrapro_275.max_wl );

    buf = get_string( "%.2f <GOTO>", 1.0e9 * wavelength );

    TRY
    {
        spectrapro_275_send( buf );
        T_free( buf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( buf );
        RETHROW( );
    }
}


/*-----------------------------------------------------------------*
 * Function asks the monochromator for the currently used grating.
 *-----------------------------------------------------------------*/

static long
spectrapro_275_get_grating( void )
{
    const char *reply;
    long gn;


    reply = spectrapro_275_talk( "?GRATING", 20 );
    gn = T_atol( reply );
    T_free( ( char * ) reply );
    return gn;
}


/*-------------------------------------------------------------------*
 * Function tells to monochromator to switch to a different grating.
 *-------------------------------------------------------------------*/

static void
spectrapro_275_set_grating( long gn )
{
    char *buf;


    fsc2_assert( gn > 0 && gn <= MAX_GRATINGS );

    if ( spectrapro_275.current_gn == gn )
        return;

    buf = get_string( "%ldGRATING", gn );

    TRY
    {
        spectrapro_275_send( buf );
        T_free( buf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( buf );
        RETHROW( );
    }
}


/*-------------------------------------------------------------------*
 * Function to set the maximum wavelength of the current grating.
 *-------------------------------------------------------------------*/

static void
spectrapro_275_store_max_wl( void )
{
    char *reply;
    char *start;
    long gr_mm;
    int i;
    

    reply = spectrapro_275_talk( "?GRATINGS", 100 );
    
    /* reply contains a list of the gratings installed, one per line.
       first we have to find the correct line, i.e. grating 1 -> line 1
       and so on. so we skip spectrapro_275.current_gn - 1 lines.

       there might (????) be an initial "\r\n" pair present so we scan
       first until we reach the first digit. */

    start = reply;
    while ( ! isdigit( ( unsigned char ) *start ) )
        start++;

    for ( i = 1; i < spectrapro_275.current_gn ; i++ )
	{
        char *eol;
	
		eol = strstr( start, "\r\n" );
		start = eol + 2;
    }
    
    /* ...now set start to the character after grating number... */

    start += 2;
    
    /* ...skip spaces... */

    while ( isspace( ( unsigned char ) *start ) )
        start++;

    /* ...and read the next number as grooves per mm */

    gr_mm = 0;
    while ( *start != '\0' && isdigit( ( unsigned char ) *start ) )
        gr_mm = gr_mm * 10 + ( long ) ( *start++ - '0' );
    
    spectrapro_275.max_wl = MAGIC_WL / gr_mm;

    T_free( reply );
}


/*-----------------------------------------------------------------------*
 * Function tries to read up to '*len' bytes of data into 'buf' from the
 * monochromator. It recognizes the end of the data (if there are less
 * than '*len' going to be send by the device) by the string " ok\r\n"
 * (or, in one case "ok\r\n") when the command initializing the read
 * was successful, or "?\r\n" when the command failed (probably due to
 * an invalid command.
 * There are four cases to be considered:
 * 1. The returned string ended in "ok\r\n" or " ok\r\n". In this case
 *    this part is cut of (i.e. replaced by a '\0') and the length of
 *    the string ready in is returned in len (could actually be 0 when
 *    the "ok\r\n" part was everything we got) and the function returns
 *    a status indicating success.
 * 2. The returned string ended ended neither in "ok\r\n", " ok\r\n" or
 *    "?\r\n", indicating that there are more data coming. In 'len' the
 *    length of what we got is returned and the function returns a
 *    status indicating failure. No '\0' is appended to the returned
 *    string.
 * 3. The string we got ended in "?\r\n", in which case the function
 *    throws an exception.
 * 4. Reading from the device failed, in which case an exception is
 *    thrown.
 * Some care has to be taken: when the input buffer 'buf' isn't large
 * enough to hold the complete string the device is trying to send it
 * may happen that the transmission ends within the marker indicating
 * success or failure, in which case this function won't be able to
 * determine if the end of a transmission has been reached. In this
 * case the calling function must do the checking!
 *
 * There's also another way this function can be ended: if the user hit
 * the "Stop" button a USER_BREAK_EXCEPTION is thrown.
 *-----------------------------------------------------------------------*/

static bool
spectrapro_275_read( char   * buf,
					 size_t * len )
{
    size_t to_fetch = *len;
    size_t already_read = 0;
    char *lbuf;
    long llen = *len;
    bool done = UNSET;


    CLOBBER_PROTECT( to_fetch );
    CLOBBER_PROTECT( already_read );
    CLOBBER_PROTECT( done );

    lbuf = T_malloc( llen );

    do
    {
        llen = to_fetch;
        if ( ! spectrapro_275_comm( SERIAL_READ, buf + already_read, &llen ) )
        {
            T_free( lbuf );
            spectrapro_275_comm_fail( );
        }

        /* Device didn't send anything yet then try again. */

        if ( llen == 0 )
            goto read_retry;

        already_read += llen;
        to_fetch -= llen;

        /* No end marker can have been read yet */

        if ( already_read < 3 )
        {
            if ( to_fetch == 0 )
                break;
            goto read_retry;
        }

        /* Throw exception if device did signal an invalid command */

        if ( ! strncmp( buf + already_read - 3, "?\r\n", 3 ) )
        {
            T_free( lbuf );
            spectrapro_275_comm_fail( );
        }

        /* No end marker indicating success can have been read yet */

        if ( already_read < 4 )
        {
            if ( to_fetch == 0 )
                break;
            goto read_retry;
        }

        /* Check if we got an indicator saying that everything the device is
           going to write has already been sent successfully - if yes replace
           the indicator by a '\0' character and break from the loop */

        if ( ! strncmp( buf + already_read - 4, "ok\r\n", 4 ) )
        {
            already_read -= 4;
            if ( already_read > 0 && buf[ already_read - 1 ] == ' ' )
                already_read--;
            buf[ already_read ] = '\0';
            done = SET;
            break;
        }

        /* When we get here we have to try again reading (more) data. Before
           we do we wait a bit and also check if the "Stop" button has been
           hit by the user. */

    read_retry:

        TRY
        {
            stop_on_user_request( );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( lbuf );
            RETHROW( );
        }
    } while ( to_fetch > 0 );

    *len = already_read;
    return done;
}


/*---------------------------------------------------------------*
 * Function sends a command and returns a buffer (with a default
 * size of *len bytes) with the reply of the device. If the
 * reply by the device is longer that *len bytes, a larger
 * buffer gets returned. The buffer always ends in a '\0'.
 *---------------------------------------------------------------*/

static char *
spectrapro_275_talk( const char * buf,
					 size_t       len )
{
    char *lbuf;
    size_t comm_len;
    size_t already_read;


    CLOBBER_PROTECT( lbuf );

    fsc2_assert( buf != NULL && *buf != '\0' && len != 0 );

    lbuf = get_string( "%s\r", buf );
    comm_len = strlen( lbuf );

    if ( ! spectrapro_275_comm( SERIAL_WRITE, lbuf ) )
    {
        T_free( lbuf );
        spectrapro_275_comm_fail( );
    }

    /* The device always echos the command, we got to get rid of this echo. */

    TRY
    {
        spectrapro_275_read( lbuf, &comm_len );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( lbuf );
        RETHROW( );
    }

    /* Now we read the reply by the device, if necessary extending the
       buffer. */

    TRY
    {
        already_read = 0;
        len += 5;
        lbuf = T_realloc( lbuf, len );

        while ( ! spectrapro_275_read( lbuf + already_read, &len ) )
        {
            lbuf = T_realloc( lbuf, 2 * len + 5 );
            already_read += len;
        }
        already_read += len;
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( lbuf );
        RETHROW( );
    }

    T_realloc( lbuf, already_read + 1 );
    return lbuf;
}


/*--------------------------------------------------------*
 * Function for commands that just get send to the device
 * and don't expect any replies.
 *--------------------------------------------------------*/

static void
spectrapro_275_send( const char * buf )
{
    char *lbuf;
    size_t len;
    char reply[ 5 ];


    fsc2_assert( buf != NULL && *buf != '\0' );

    lbuf = get_string( "%s\r", buf );
    len = strlen( lbuf );

    if ( ! spectrapro_275_comm( SERIAL_WRITE, lbuf ) )
    {
        T_free( lbuf );
        spectrapro_275_comm_fail( );
    }

    /* The device always echos the command, we got to get rid of the echo */

    TRY
    {
        spectrapro_275_read( lbuf, &len );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( lbuf );
        RETHROW( );
    }

    T_free( lbuf );

    /* Read the string returned by the device indicating success */

    len = 5;
    if ( ! spectrapro_275_read( reply, &len ) )
        spectrapro_275_comm_fail( );
}


/*-----------------------------------------------------------------*
 * Low level function for the communication with the monochromator
 * via the serial port.
 *-----------------------------------------------------------------*/

static bool
spectrapro_275_comm( int type,
					 ... )
{
    va_list ap;
    char *buf;
    ssize_t len;
    size_t *lptr;


    switch ( type )
    {
        case SERIAL_INIT :
            /* Open the serial port for reading and writing. */

            if ( ( spectrapro_275.tio = fsc2_serial_open( spectrapro_275.sn,
                                                          O_RDWR ) ) == NULL )
                return FAIL;

            /* Set transfer mode to 8 bit, no parity and 1 stop bit (8-N-1)
               and ignore control lines, don't use flow control. */

            spectrapro_275.tio->c_cflag = 0;
            spectrapro_275.tio->c_cflag = CLOCAL | CREAD | CS8;
            spectrapro_275.tio->c_iflag = IGNBRK;
            spectrapro_275.tio->c_oflag = 0;
            spectrapro_275.tio->c_lflag = 0;
            cfsetispeed( spectrapro_275.tio, SERIAL_BAUDRATE );
            cfsetospeed( spectrapro_275.tio, SERIAL_BAUDRATE );

            fsc2_tcflush( spectrapro_275.sn, TCIFLUSH );
            fsc2_tcsetattr( spectrapro_275.sn, TCSANOW, spectrapro_275.tio );
            break;

        case SERIAL_EXIT :                    /* reset and close serial port */
            fsc2_serial_close( spectrapro_275.sn );
            break;

        case SERIAL_WRITE :
            va_start( ap, type );
            buf = va_arg( ap, char * );
            va_end( ap );

            len = strlen( buf );
            if ( fsc2_serial_write( spectrapro_275.sn, buf, len,
                                    0, UNSET ) != len )
                return FAIL;
            break;

        case SERIAL_READ :
            va_start( ap, type );
            buf = va_arg( ap, char * );
            lptr = va_arg( ap, size_t * );
            va_end( ap );

            len = fsc2_serial_read( spectrapro_275.sn, buf, *lptr, NULL,
                                    SPECTRAPRO_275_WAIT, UNSET );
            if ( len < 0 )
                return FAIL;
            else
                *lptr = len;
            break;

        default :
            print( FATAL, "INTERNAL ERROR detected at %s:%d.\n",
                   __FILE__, __LINE__ );
            THROW( EXCEPTION );
    }

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
spectrapro_275_open( void )
{
    char reply[ 100 ];
    size_t len;
    size_t already_read = 0;
    int i;


    if ( ! spectrapro_275_comm( SERIAL_INIT ) )
    {
        print( FATAL, "Can't open device file for monochromator.\n" );
        THROW( EXCEPTION );
    }

    spectrapro_275.is_open = SET;

    /* Now a quick check that we can talk to the monochromator, it should be
       able to send us the list of gratings installed within one second or
       something is definitely hosed... */

    if ( ! spectrapro_275_comm( SERIAL_WRITE, "?GRATINGS\r" ) )
        spectrapro_275_comm_fail( );

    for ( i = 0; i < 10; i++ )
    {
        len = 100 - already_read;
        if ( spectrapro_275_comm( SERIAL_READ, reply + already_read, &len ) )
        {
            already_read += len;
            if (    already_read >= 5
                 && ! strncmp( reply + already_read - 5, " ok\r\n", 5 ) )
                break;
        }
        stop_on_user_request( );
    }

    if ( i == 10 )
        spectrapro_275_comm_fail( );

}


/*--------------------------------------------------------------*
 * Function just closes the device file for the serial port the
 * monochromator is attached to.
 *--------------------------------------------------------------*/

static void
spectrapro_275_close( void )
{
    if ( spectrapro_275.is_open )
        spectrapro_275_comm( SERIAL_EXIT );
    spectrapro_275.is_open = UNSET;
}


/*----------------------------------------------------------*
 * Function called on communication failure with the device
 *----------------------------------------------------------*/


static void
spectrapro_275_comm_fail( void )
{
    print( FATAL, "Can't access the monochromator.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
