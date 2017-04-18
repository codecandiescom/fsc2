/*
 *  Copyright (C) 1999-2017 Jens Thoms Toerring
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
#include <sys/time.h>
#include <float.h>
#include "serial.h"

#include "thorlabs_fw212c.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


enum {
    SERIAL_INIT,
    SERIAL_READ,
    SERIAL_WRITE,
    SERIAL_EXIT
};


/* Hook functions */

int thorlabs_fw212c_init_hook( void );
int thorlabs_fw212c_test_hook( void );
int thorlabs_fw212c_end_of_test_hook( void );
int thorlabs_fw212c_exp_hook( void );
int thorlabs_fw212c_end_of_exp_hook( void );


/* EDL functions */

Var_T * filterwheel_name( Var_T * v  UNUSED_ARG );
Var_T * filterwheel_position( Var_T * v );
Var_T * filterwheel_speed( Var_T * v );

/* Internal functions */

static bool thorlabs_fw212c_init( void );
static long thorlabs_fw212c_get_position( void );
static long thorlabs_fw212c_set_position( long position );
static long thorlabs_fw212c_get_speed( void );
static long thorlabs_fw212c_set_speed( long spd );
static long thorlabs_fw212c_talk( const char * cmd,
                                  char       * reply,
                                  long         length,
                                  long         timeout );
static void thorlabs_fw212c_failure( void );
static bool thorlabs_fw212c_serial_comm( int type,
                                         ... );

/* Global variables */

typedef struct
{
    int              handle;
    long             position;
    long             positions;
    int              speed;
    struct termios * tio;        /* serial port terminal interface structure */
} ThorLabs_FW212C;


static ThorLabs_FW212C thorlabs_fw212c,
                       thorlabs_fw212c_test;
static ThorLabs_FW212C * tf;


/* Defines */

#define TEST_POSITION 0


/*---------------------------------------------------*
 * Init hook, called when the module is loaded
 *---------------------------------------------------*/

int
thorlabs_fw212c_init_hook( void )
{
    tf = &thorlabs_fw212c;

    tf->handle = fsc2_request_serial_port( DEVICE_FILE, DEVICE_NAME );
    tf->position = TEST_POSITION;

    return 1;
}


/*---------------------------------------------------*
 * Test hook, called at the start of the test run
 *---------------------------------------------------*/

int
thorlabs_fw212c_test_hook( void )
{
    thorlabs_fw212c_test = thorlabs_fw212c;
    tf = &thorlabs_fw212c_test;

    return 1;
}


/*---------------------------------------------------*
 * End-of-test hook, called after the end of the test run
 *---------------------------------------------------*/

int
thorlabs_fw212c_end_of_test_hook( void )
{
    tf = &thorlabs_fw212c;
    return 1;
}


/*---------------------------------------------------*
 * Exp hook, called at the start of the experiment
 *---------------------------------------------------*/

int
thorlabs_fw212c_exp_hook( void )
{
    return thorlabs_fw212c_init( );
}


/*---------------------------------------------------*
 * End-of-exp hook, called at the end of the experiment
 *---------------------------------------------------*/

int
thorlabs_fw212c_end_of_exp_hook( void )
{
    thorlabs_fw212c_serial_comm( SERIAL_EXIT );
    return 1;
}


/*-------------------------------------------------------*
 * Function returns a string with the name of the device
 *-------------------------------------------------------*/

Var_T *
filterwheel_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}



/*-------------------------------------------------------*
 * Sets a new position
 *-------------------------------------------------------*/

Var_T *
filterwheel_position( Var_T * v )
{
    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            tf->position = thorlabs_fw212c_get_position( );
        return vars_push( INT_VAR, tf->position );
    }

    long pos = get_strict_long( v, "filterwheel position" );

    if ( pos < 1 || pos > NUM_OCCUPIED_POSITIONS )
    {
        print( FATAL, "Invalid position %ld, must be between 1 and %d.\n",
               pos, NUM_OCCUPIED_POSITIONS );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        pos = thorlabs_fw212c_set_position( pos );

    return vars_push( INT_VAR, tf->position = pos );
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

Var_T *
filterwheel_speed( Var_T * v )
{
    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            tf->speed = thorlabs_fw212c_get_speed( );
        return vars_push( INT_VAR, tf->speed );
    }

    long spd = get_strict_long( v, "filterwheel speed" );
    if (spd != 0 && spd != 1)
    {
        print( FATAL, "Invalid filterwheel speed, only 0 or 1 are "
                         "allowed\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT && spd != thorlabs_fw212c_get_speed( ) )
        spd = thorlabs_fw212c_set_speed( spd );

    return vars_push( INT_VAR, tf->speed = spd );
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static bool
thorlabs_fw212c_init( void )
{
    char buf[ 50 ];
    char *eptr;


    if ( ! thorlabs_fw212c_serial_comm( SERIAL_INIT ) )
    {
        print( FATAL, "failed to connect to device.\n" );
        THROW( EXCEPTION );
    }

	/* Ask for the number of positions and make sure it's not smaller
       than the number of occupied positions specified in the configrarion
       file */

    strcpy(buf, "pcount?\r");
    thorlabs_fw212c_talk( buf, buf, sizeof buf, READ_TIMEOUT );

    errno = 0;
    tf->positions = strtol( buf, &eptr, 10 );
    if ( eptr == buf || errno || tf->positions < 1 )
    {
        print( FATAL, "device sent invalid reply for number of positions.\n" );
        THROW( EXCEPTION );
    }

    if ( tf->positions < NUM_OCCUPIED_POSITIONS )
    {
        print( FATAL, "device has less positions (%ld) than number of occupied "
               "positions set in configuration file (%ld).\n",
               tf->positions, NUM_OCCUPIED_POSITIONS );
        THROW( EXCEPTION );
    }

    /* Get the speed, which should be 0 (for slow) or 1 (for fast) */

    strcpy( buf, "speed?\r" );
    thorlabs_fw212c_talk( buf, buf, sizeof buf, READ_TIMEOUT );

    errno = 0;
    if ( buf[ 0 ] != '0' && buf[ 0 ] != '1' )
    {
        print( FATAL, "device sent invalid reply for speed.\n" );
        THROW( EXCEPTION );
    }

    tf->speed = buf[ 0 ] = 1;

    /* Get the current position */

    tf->position = thorlabs_fw212c_get_position( );

    return true;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static long
thorlabs_fw212c_get_position(void)
{
    char buf[ 50 ] = "pos?\r";
    char *eptr;
    long pos;

    thorlabs_fw212c_talk(buf, buf, sizeof buf, READ_TIMEOUT );

    errno = 0;
    pos = strtol( buf, &eptr, 10 );
    if (    eptr == buf || errno
         || pos < 1 || pos > tf->positions )
    {
        print( FATAL, "device sent invalid reply for position.\n" );
        THROW( EXCEPTION );
    }

    return pos;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static long
thorlabs_fw212c_set_position( long pos )
{
    char buf[ 50 ];


    fsc2_assert( pos >= 1 && pos <= NUM_OCCUPIED_POSITIONS );

    /* Gues how long it should take to reach the new position. With slow
       speed it will tale about 2 seconds per position, with fast speed
       about 1 s. The device automatically picks the shortest direction.
       Add a factor of 2 to be on the safe side. */

    long p_max = l_max( pos, tf->position );
    long p_min = l_min( pos, tf->position );

    long timeout = l_min( p_max - p_min, tf->positions + p_min - p_max );
    timeout *= 2 * 1000000 * ( tf->speed + 2 );

    sprintf( buf, "pos=%ld\r", pos );
    thorlabs_fw212c_talk( buf, buf, sizeof buf, timeout );

    return pos;
}
                         

/*---------------------------------------------------*
 *---------------------------------------------------*/

static long
thorlabs_fw212c_get_speed(void)
{
    char buf[ 50 ] = "speed?\r";
    char *eptr;
    long spd;

    thorlabs_fw212c_talk(buf, buf, sizeof buf, READ_TIMEOUT );

    spd = strtol( buf, &eptr, 10 );
    if (    eptr == buf || errno || spd < 0 || spd > 1 )
    {
        print( FATAL, "device sent invalid reply for position.\n" );
        THROW( EXCEPTION );
    }

    return spd;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static long
thorlabs_fw212c_set_speed( long spd )
{
    char buf[ 50 ];

    sprintf( buf, "speed=%ld\r", spd );
    thorlabs_fw212c_talk( buf, buf, sizeof buf, READ_TIMEOUT );

    return spd;
}


/*---------------------------------------------------*
 * Sends a single string to the device and then reads
 * in the device's reply. The input buffer must have room
 *  for the replY (including a carriage return , '>' and
 * space at the end). The trailing stuff is removed and
 * the returned string is nul-terminated.
 * Take care: the device echos each command you send
 * it, so this has to be removed from the reply to
 * queries! Also keep in mind that the 'reply'
 * buffer must be large enough for the echoed
 * command.
 *---------------------------------------------------*/

static
long
thorlabs_fw212c_talk( const char * cmd,
                      char       * reply,
                      long         length,
                      long         timeout )
{
    size_t cnt = strlen( cmd );

    fsc2_assert( length >= 3 );

    if (cmd[ cnt - 2 ] != '?')
        cnt = 0;

    if ( ! thorlabs_fw212c_serial_comm( SERIAL_WRITE, cmd ) )
        thorlabs_fw212c_failure( );

    if (    ! thorlabs_fw212c_serial_comm( SERIAL_READ, reply,
                                           &length, timeout )
         || length < 3
         || strncmp( reply + length - 3, "\r> ", 3 ) )
        thorlabs_fw212c_failure( );

    reply[ length -= 3 ] = '\0';

    if (cnt)
        memcpy(reply, reply + cnt, length -= cnt);
    return length;
}


/*---------------------------------------------------*
 * Called whenever the communication with the device fails,
 * either because the device doesn't react in time or since
 * it sent invalid data.
 *---------------------------------------------------*/

static
void
thorlabs_fw212c_failure( void )
{
    print( FATAL, "Communication with powermeter failed.\n" );
    THROW( EXCEPTION );
}


/*---------------------------------------------------*
 * Function for opening and closing a connection as well
 * as reading or writing data when the the device is
 * connected via the serial port or USB (the latter just
 * being a USB-to-serial converter)
 *---------------------------------------------------*/

static bool
thorlabs_fw212c_serial_comm( int type,
                             ... )
{
    va_list ap;
    char * buf;
    ssize_t len;
    size_t * lptr;
    long timeout;


    switch ( type )
    {
        case SERIAL_INIT :
            /* Open the serial port for reading and writing. */

            if ( ( tf->tio = fsc2_serial_open( tf->handle, O_RDWR ) ) == NULL )
                return false;

            /* Use 8-N-1, ignore flow control and modem lines, enable
               reading and set the baud rate. */

            memset(tf->tio, 0, sizeof *tf->tio);
            tf->tio->c_cflag = CS8 | CLOCAL | CREAD;
            tf->tio->c_iflag = 0; //IGNBRK;
            tf->tio->c_oflag = 0;
            tf->tio->c_lflag = 0;
            
            cfsetispeed( tf->tio, SERIAL_BAUDRATE );
            cfsetospeed( tf->tio, SERIAL_BAUDRATE );

            fsc2_tcflush( tf->handle, TCIOFLUSH );
            fsc2_tcsetattr( tf->handle, TCSANOW, tf->tio );
            break;

        case SERIAL_EXIT :
            fsc2_serial_close( tf->handle );
            break;

        case SERIAL_WRITE :
            va_start( ap, type );
            buf = va_arg( ap, char * );
            va_end( ap );

            len = strlen( buf );
            if ( fsc2_serial_write( tf->handle, buf, len,
                                    WRITE_TIMEOUT, true ) != len )
            {
                if ( len == 0 )
                    stop_on_user_request( );
                return false;
            }
            break;

        case SERIAL_READ :
            va_start( ap, type );
            buf = va_arg( ap, char * );
            lptr = va_arg( ap, size_t * );
            timeout = va_arg( ap, long );
            va_end( ap );

            /* Try to read from the device (reads need to stop on "\r> ") */

            if ( ( *lptr = fsc2_serial_read( tf->handle, buf, *lptr,
                                             "\r> ", timeout, false ) )
                 <= 0 )
            {
                if ( *lptr == 0 )
                    stop_on_user_request( );

                return false;
            }
            break;

        default :
            print( FATAL, "INTERNAL ERROR detected at %s:%d.\n",
                   __FILE__, __LINE__ );
            THROW( EXCEPTION );
    }

    return true;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
