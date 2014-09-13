/*
 *  Copyright (C) 2002-2014  Jens Thoms Toerring
 * 
 *  This library should simplify accessing NI6601 GBCT boards by National
 *  Instruments by avoiding to be forced to make ioctl() calls and use
 *  higher level functions instead.
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 *  To contact the author send email to:  jt@toerring.de
 */


#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "ni6601.h"

#define NI6601_DEVICE_NAME "ni6601_"


static int ni6601_is_armed( int   /* board   */,
                            int   /* counter */,
                            int * /* state   */ );

static int ni6601_state( int   /* board   */,
                         int   /* counter */,
                         int * /* state   */ );

static int check_board( int /* board */ );

static int check_source( int /* source */ );

static int ni6601_time_to_ticks( double          /* time  */,
                                 unsigned long * /* ticks */ );


static NI6601_Device_Info dev_info[ NI6601_MAX_BOARDS ];
static const char *error_message = "";


#define DEFAULT_NUM_POINTS  1024

static int ni6601_errno = 0;

const char *ni6601_errlist[ ] = {
    "Success",                                       /* NI6601_OK      */
    "No such board",                                 /* NI6601_ERR_NSB */
    "No such counter",                               /* NI6601_ERR_NSC */
    "Counter is busy",                               /* NI6601_ERR_CBS */
    "Invalid argument",                              /* NI6601_ERR_IVA */
    "Can't wait for continuous counter to stop",     /* NI6601_ERR_WFC */
    "Board is busy",                                 /* NI6601_ERR_BBS */
    "Source argument invalid",                       /* NI6601_ERR_IVS */
    "Board not open",                                /* NI6601_ERR_BNO */
    "No driver loaded for board",                    /* NI6601_ERR_NDV */
    "Neighbouring counter is busy",                  /* NI6601_ERR_NCB */
    "Interrupted by signal",                         /* NI6601_ERR_ITR */
    "No permissions to open device file",            /* NI6601_ERR_ACS */
    "Device file does not exist",                    /* NI6601_ERR_DFM */
    "Unspecified error when opening device file",    /* NI6601_ERR_DFP */
    "Internal driver or library error",              /* NI6601_ERR_INT */
    "Only one buffered counter possible",            /* NI6601_ERR_TMB */
    "Not enough memory for internal buffers",        /* NI6601_ERR_MEM */
    "Internal buffer overflow",                      /* NI6601_ERR_OFL */
    "Acquisition is procceding too fast",            /* NI6601_ERR_TFS */
    "No buffered counter running"                    /* NI6601_ERR_NBC */
};

const int ni6601_nerr =
                ( int ) ( sizeof ni6601_errlist / sizeof ni6601_errlist[ 0 ] );


/*-----------------------------------------------------------------
 *-----------------------------------------------------------------*/

int ni6601_close( int board )
{
    int i;


    if ( board < 0 || board >= NI6601_MAX_BOARDS )
        return ni6601_errno = NI6601_ERR_NSB;

    if ( ! dev_info[ board ].is_init )
        return ni6601_errno = NI6601_ERR_BNO;

    for ( i = NI6601_COUNTER_0; i <= NI6601_COUNTER_3; i++ )
        if ( dev_info[ board ].state[ i ] == NI6601_BUFF_COUNTER_RUNNING )
            break;

    if ( dev_info[ board ].fd >= 0 )
        while ( close( dev_info[ board ].fd ) && errno == EINTR )
            /* empty */ ;

    dev_info[ board ].is_init = 0;

    return ni6601_errno = NI6601_OK;
}


/*-----------------------------------------------------------------*
 * Function starts a counter that will run until it is explicitely
 * stopped by a call to ni6601_stop_counter().
 *-----------------------------------------------------------------*/

int ni6601_start_counter( int board,
                          int counter,
                          int source )
{
    int ret;
    int state;
    NI6601_COUNTER c;


    if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
        return ni6601_errno = NI6601_ERR_NSC;

    if ( ( ret = check_board( board ) ) < 0 )
        return ret;

    if ( ( ret = check_source( source ) ) < 0 )
        return ret;

    if ( ( ret = ni6601_state( board, counter, &state ) ) < 0 )
        return ret;

    if ( state == NI6601_BUSY )
        return ni6601_errno = NI6601_ERR_CBS;

    c.counter = counter;
    c.source_polarity = NI6601_NORMAL;
    c.gate = NI6601_NONE;
    c.source = source;

    if ( ioctl( dev_info[ board ].fd, NI6601_IOC_COUNTER, &c ) < 0 )
        return ni6601_errno = NI6601_ERR_INT;

    dev_info[ board ].state[ counter ] = NI6601_CONT_COUNTER_RUNNING;

    return ni6601_errno = NI6601_OK;
}


/*----------------------------------------------------------------------*
 * Function starts a counter that will be gated by its adjacent counter
 * for 'gate_length'. It stops automatically at the end of the gate.
 *----------------------------------------------------------------------*/

int ni6601_start_gated_counter( int    board,
                                int    counter,
                                double gate_length,
                                int    source )
{
    int ret;
    int pulser;
    int state;
    unsigned long len;
    NI6601_PULSES p;
    NI6601_COUNTER c;


    /* Check if the counter number is reasonable and determine number of
       adjacent counter to be used for gating */

    if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
        return ni6601_errno = NI6601_ERR_NSC;

    pulser = counter + ( counter & 1 ? - 1 : 1 );

    /* Check that the gate length is reasonable and convert it to ticks */

    if ( ni6601_time_to_ticks( gate_length, &len ) < 0 )
        return ni6601_errno = NI6601_ERR_IVA;

    /* Open the board if necessary */

    if ( ( ret = check_board( board ) ) < 0 )
        return ret;

    /* Check that the counters source is valid */

    if ( ( ret = check_source( source ) ) < 0 )
        return ret;

    /* Check that neither the counter nor the adjacent counter is busy */

    if ( ( ret = ni6601_state( board, counter, &state ) ) < 0 )
        return ret;

    if ( state == NI6601_BUSY )
        return ni6601_errno = NI6601_ERR_CBS;

    if ( ( ret = ni6601_state( board, pulser, &state ) ) < 0 )
        return ret;

    if ( state == NI6601_BUSY )
        return ni6601_errno = NI6601_ERR_NCB;

    /* Start the counter */

    c.counter = counter;
    c.source_polarity = NI6601_NORMAL;
    c.gate = NI6601_NEXT_OUT;
    c.source = source;

    if ( ioctl( dev_info[ board ].fd, NI6601_IOC_COUNTER, &c ) < 0 )
        return ni6601_errno = NI6601_ERR_INT;

    /* Start the adjacent counter producing the gate */

    p.counter = pulser;
    p.continuous = 0;
    p.low_ticks = 2;
    p.high_ticks = len;
    p.source = NI6601_TIMEBASE_1;
    p.gate = NI6601_NONE;
    p.disable_output = 0;
    p.output_polarity = NI6601_NORMAL;

    if ( ioctl( dev_info[ board ].fd, NI6601_IOC_PULSER, &p ) < 0 )
    {
        ni6601_stop_counter( board, counter );
        return ni6601_errno = NI6601_ERR_INT;
    }

    dev_info[ board ].state[ counter ] = NI6601_COUNTER_RUNNING;
    dev_info[ board ].state[ pulser ]  = NI6601_PULSER_RUNNING;

    return ni6601_errno = NI6601_OK;
}


/*----------------------------------------------------------------------*
 * Function starts a counter that will repeatedly count on the gate
 * provided by its adjacent counter for 'gate_length'. It stops auto-
 * matically after 'num_points' have been counted or, if 'num_points'
 * is 0, will keep counting again and again until the function
 * ni6601_stop_fast_gated_counter() is called. The data must be
 * fetched by calling read(2) on the device file.
 *----------------------------------------------------------------------*/

int ni6601_start_buffered_counter( int           board,
                                   int           counter,
                                   double        gate_length,
                                   int           source,
                                   unsigned long num_points,
                                   int           continuous )
{
    int ret;
    int pulser;
    int state;
    unsigned long len;
    NI6601_PULSES p;
    NI6601_BUF_COUNTER c;
    int i;


    /* Check if the counter number is reasonable and determine number of
       adjacent counter to be used for gating */

    if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
        return ni6601_errno = NI6601_ERR_NSC;

    if ( ! continuous && num_points == 0 )
        return ni6601_errno = NI6601_ERR_IVA;

    pulser = counter + ( counter & 1 ? - 1 : 1 );

    /* Check that the gate length is reasonable and convert it to ticks */

    if ( ni6601_time_to_ticks( gate_length, &len ) < 0 || len < 3 )
        return ni6601_errno = NI6601_ERR_IVA;

    /* Open the board if necessary */

    if ( ( ret = check_board( board ) ) < 0 )
        return ret;

    /* Check that the counters source is valid */

    if ( ( ret = check_source( source ) ) < 0 )
        return ret;

    /* Check that neither the counter nor the adjacent counter is busy */

    if ( ( ret = ni6601_state( board, counter, &state ) ) < 0 )
        return ret;

    if ( state == NI6601_BUSY )
        return ni6601_errno = NI6601_ERR_CBS;

    if ( ( ret = ni6601_state( board, pulser, &state ) ) < 0 )
        return ret;

    if ( state == NI6601_BUSY )
        return ni6601_errno = NI6601_ERR_NCB;

    /* Check that no counter is already running as a buffered counter */

    for ( i = NI6601_COUNTER_0; i <= NI6601_COUNTER_3; i++ )
    {
        if ( ( ret = ni6601_state( board, i, &state ) ) < 0 )
            return ret;
        if ( state == NI6601_BUFF_COUNTER_RUNNING )
            return ni6601_errno = NI6601_ERR_TMB;
    }

    /* Start the counter */

    c.counter = counter;
    c.source_polarity = NI6601_NORMAL;
    c.gate = NI6601_NEXT_OUT;
    c.source = source;

    /* If for continuous mode a zero number of points is given make the
       buffer large enough for 1s worth of data points. Make sure that
       the buffer is not smaller than a default value (currently 1k) */

    if ( continuous ) {
        if ( num_points == 0 ) {
            c.num_points = ( unsigned long ) ( 1.0 / gate_length );
            if ( c.num_points < DEFAULT_NUM_POINTS )
                c.num_points = DEFAULT_NUM_POINTS;
        }
        else
            c.num_points = num_points;
        c.continuous = 1;
    }
    else
    {
        c.num_points = num_points;
        c.continuous = 0;
    }

    if ( ioctl( dev_info[ board ].fd, NI6601_IOC_START_BUF_COUNTER, &c ) < 0 )
        return ni6601_errno = NI6601_ERR_INT;

    dev_info[ board ].state[ counter ] = NI6601_BUFF_COUNTER_RUNNING;

    /* Start the adjacent counter producing the gate */

    p.counter = pulser;
    p.continuous = 1;
    p.low_ticks = 2;
    p.high_ticks = len - 1;
    p.source = NI6601_TIMEBASE_1;
    p.gate = NI6601_NONE;
    p.disable_output = 0;
    p.output_polarity = NI6601_NORMAL;

    if ( ioctl( dev_info[ board ].fd, NI6601_IOC_PULSER, &p ) < 0 )
    {
        ni6601_stop_counter( board, counter );
        return ni6601_errno = NI6601_ERR_INT;
    }

    dev_info[ board ].state[ pulser ] = NI6601_PULSER_RUNNING;

    return ni6601_errno = NI6601_OK;
}


/*------------------------------------------------------------------------*
 * Function for determing how many data (in bytes) to be returned by the
 * a call of buffered counter (since more data can be acquired in the
 * time between the call of this function and actually fetching the date
 * this should only be used as an estimate).
 *------------------------------------------------------------------------*/

ssize_t ni6601_get_buffered_available( int board )
{
    int i;
    NI6601_BUF_AVAIL a;
    int ret;


    /* Check that the board is open */

    if ( ( ret = check_board( board ) ) < 0 )
        return ret;

    /* We can get values only from a buffered running counter, the number of
       points must be at least 1, the buffer for storing the data can't be a
       NULL pointer and the waiting time, if expressed in microseconds, not
       larger than what fits into a long value */

    for ( i = NI6601_COUNTER_0; i <= NI6601_COUNTER_3; i++ )
        if ( dev_info[ board ].state[ i ] == NI6601_BUFF_COUNTER_RUNNING )
            break;

    if ( i > NI6601_COUNTER_3 )
        return ni6601_errno = NI6601_ERR_NBC;

    /* Get the count - the only thing that can go wrong is having no buffered
       counter running and that would indicate at an error in the library... */

    if ( ioctl( dev_info[ board ].fd, NI6601_IOC_GET_BUF_AVAIL, &a ) < 0 )
        return ni6601_errno = NI6601_ERR_INT;

    return ( ssize_t ) a.count / 4;
}


/*------------------------------------------------------------------------*
 * Function to fetch data from a buffered counter
 *------------------------------------------------------------------------*/

ssize_t ni6601_get_buffered_counts( int             board,
                                    unsigned long * counts,
                                    size_t          num_points,
                                    double          wait_secs,
                                    int *           quit_on_signal,
                                    int *           timed_out,
                                    int *           end_of_data )
{
    fd_set rfds;
    unsigned char *buf;
    int sret;
    ssize_t ret;
    ssize_t i;
    long us_wait;
    ssize_t remaining = num_points * 4,
            transfered = 0;
    struct timeval before,
                   after,
                   timeout;
    int got_signal = 0;


    if ( timed_out != NULL )
        *timed_out = 0;
    if ( end_of_data != NULL )
        *end_of_data = 0;

    /* Check that the board is open */

    if ( ( ret = check_board( board ) ) < 0 )
        return ret;

    /* We can get values only from a buffered running counter, the number of
       points must be at least 1, the buffer for storing the data can't be a
       NULL pointer and the waiting time, if expressed in microseconds, not
       larger than what fits into a long value */

    for ( i = NI6601_COUNTER_0; i <= NI6601_COUNTER_3; i++ )
        if ( dev_info[ board ].state[ i ] == NI6601_BUFF_COUNTER_RUNNING )
            break;

    if ( i > NI6601_COUNTER_3 )
        return ni6601_errno = NI6601_ERR_NBC;

    if ( num_points == 0 || counts == NULL ||
         ( wait_secs >= 0.0 && floor( wait_secs * 1.0e6 + 0.5 ) > LONG_MAX ) )
        return ni6601_errno = NI6601_ERR_IVA;

    /* Get enough memory for an intermediate buffer (we don't really know
       the hardware this is going to run on, so we can't assume that the
       endian-ness and sizes of the user supplied buffer is identical to
       what we get from the card, so let's play it safe as far as possible -
       it probably still won't work on a machine where CHAR_BIT isn't 8...) */

    if ( ( buf = malloc( remaining ) ) == NULL )
        return ni6601_errno = NI6601_ERR_MEM;

    /* First deal with the case that the caller wants results immediately,
       with no waiting for data if there are none yet */

    if ( wait_secs < 0.0 )
    {
      read_retry:

        ret = read( dev_info[ board ].fd, buf, remaining );

        if ( ret < 0 )
            switch ( errno )
            {
                case EAGAIN :
                    free( buf );
                    return 0;

                case EINTR :
                    if ( *quit_on_signal )
                    {
                        got_signal = 1;
                        free( buf );
                        return 0;
                    }
                    else
                        goto read_retry;

                case EOVERFLOW :
                    free( buf );
                    return ni6601_errno = NI6601_ERR_OFL;
            
                case ESTRPIPE :
                    free( buf );
                    return ni6601_errno = NI6601_ERR_TFS;

                default :
                    free( buf );
                    return ni6601_errno = NI6601_ERR_INT;
            }

        if ( ret == 0 )       /* no more data available from board */
        {
            *end_of_data = 1;
            free( buf );
            return 0;
        }

        /* Data we got from the driver are 4-byte small-endian */

        for ( i = 0; i < ret / 4; i++ )
            counts[ i ] =              buf[ 4 * i     ]
                          +      256 * buf[ 4 * i + 1 ]
                          +    65536 * buf[ 4 * i + 2 ]
                          + 16777216 * buf[ 4 * i + 3 ];

        free( buf );
        if ( quit_on_signal != NULL )
            *quit_on_signal = got_signal;
        return ret / 4;
    }

    /* Now deal with the case that the caller specified a timeout (possibly
       an infinite one by passing 0.0 as the time to wait) */

    us_wait = ( long ) floor( wait_secs * 1.0e6 + 0.5 );

    while ( remaining > 0 )
    {
        FD_ZERO( &rfds );
        FD_SET( dev_info[ board ].fd, &rfds );

        if ( wait_secs > 0.0 )
        {
            if ( us_wait <= 0 )
            {
                *timed_out = 1;
                break;
            }

            timeout.tv_sec  = us_wait / 1000000;
            timeout.tv_usec = us_wait % 1000000;
            gettimeofday( &before, NULL );
        }

        sret = select( dev_info[ board ].fd + 1, &rfds, NULL,
                       NULL, wait_secs > 0.0 ? &timeout : NULL );

        if ( sret < 0 )
        {
            if ( errno == EINTR )
            {
                if ( *quit_on_signal )
                {
                    got_signal = 1;
                    break;
                }
                else
                {
                    if ( wait_secs > 0.0 )
                    {
                        gettimeofday( &after, NULL );               
                        us_wait -=   (   after.tv_sec * 1000000
                                       + after.tv_usec  )
                                   - (   before.tv_sec * 1000000 
                                       + before.tv_usec );
                        }
                    continue;
                }
            }

            free( buf );
            return ni6601_errno = NI6601_ERR_INT;
        }

        if ( sret == 0 )           /* Timeout */
        {
            *timed_out = 1;
            break;
        }

        if ( ! FD_ISSET( dev_info[ board ].fd, &rfds ) )   /* wrong fd ready */
        {
            if ( wait_secs > 0.0 )
            {
                gettimeofday( &after, NULL );               
                us_wait -=   ( after.tv_sec  * 1000000 + after.tv_usec  )
                           - ( before.tv_sec * 1000000 + before.tv_usec );
            }
            continue;
        }

        ret = read( dev_info[ board ].fd, buf + transfered, remaining );

        if ( ret < 0 )
            switch ( errno )
            {
                case EINTR :
                    if ( *quit_on_signal )
                    {
                        got_signal = 1;
                        ret = 0;
                        break;
                    }
                    else
                    {
                        if ( wait_secs > 0.0 )
                        {
                            gettimeofday( &after, NULL );               
                            us_wait -=   (   after.tv_sec * 1000000
                                           + after.tv_usec  )
                                       - (   before.tv_sec * 1000000 
                                           + before.tv_usec );
                        }
                        continue;
                    }

                case EOVERFLOW :
                    free( buf );
                    return ni6601_errno = NI6601_ERR_OFL;
            
                case ESTRPIPE :
                    free( buf );
                    return ni6601_errno = NI6601_ERR_TFS;

                default :
                    free( buf );
                    return ni6601_errno = NI6601_ERR_INT;
            }

        if ( ret == 0 )
        {
            *end_of_data = 1;
            break;
        }

        transfered += ret;
        remaining -= ret;

        if ( wait_secs > 0.0 )
        {
            gettimeofday( &after, NULL );               
            us_wait -=   ( after.tv_sec  * 1000000 + after.tv_usec  )
                       - ( before.tv_sec * 1000000 + before.tv_usec );
        }
    }

    for ( i = 0; i < transfered / 4; i++ )
        counts[ i ] =              buf[ 4 * i     ]
                      +      256 * buf[ 4 * i + 1 ]
                      +    65536 * buf[ 4 * i + 2 ]
                      + 16777216 * buf[ 4 * i + 3 ];

    free( buf );
    if ( quit_on_signal != NULL )
        *quit_on_signal = got_signal;
    return transfered / 4;
}


/*------------------------------------------------------------------------*
 * Function to stop a running counter (and, if necessary the accompanying
 * pulser in case of continuously runnning or buffered counters)
 *------------------------------------------------------------------------*/

int ni6601_stop_counter( int board,
                         int counter )
{
    int ret;
    int state;


    if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
        return ni6601_errno = NI6601_ERR_NSC;

    if ( ( ret = check_board( board ) ) < 0 )
        return ret;

    if ( ( ret = ni6601_state( board, counter, &state ) ) < 0 )
        return ret;

    if ( state == NI6601_IDLE )
        return ni6601_errno = NI6601_OK;

    if ( dev_info[ board ].state[ counter ] == NI6601_BUFF_COUNTER_RUNNING )
    {
        if ( ioctl( dev_info[ board ].fd, NI6601_IOC_STOP_BUF_COUNTER ) < 0 )
            return ni6601_errno = NI6601_ERR_INT;
    }
    else
    {
        NI6601_DISARM d = { counter };

        if ( ioctl( dev_info[ board ].fd, NI6601_IOC_DISARM, &d ) < 0 )
            return ni6601_errno = NI6601_ERR_INT;
    }

    if ( dev_info[ board ].state[ counter ] == NI6601_COUNTER_RUNNING ||
         dev_info[ board ].state[ counter ] == NI6601_BUFF_COUNTER_RUNNING )
    {
        int pulser = counter + ( counter & 1 ? -1 : 1 );

        if ( dev_info[ board ].state[ pulser ] == NI6601_PULSER_RUNNING )
            ni6601_stop_pulses( board, pulser );
    }

    dev_info[ board ].state[ counter ] = NI6601_IDLE;

    return ni6601_errno = NI6601_OK;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

int ni6601_get_count( int             board,
                      int             counter,
                      int             wait_for_end,
                      int             do_poll,
                      unsigned long * count,
                      int *           state )
{
    int ret;
    NI6601_COUNTER_VAL v;


    if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
        return ni6601_errno = NI6601_ERR_NSC;

    /* Check that the board is open */

    if ( ( ret = check_board( board ) ) < 0 )
        return ret;

    /* We can get values from a buffered counter only using read */

    if ( dev_info[ board ].state[ counter ] == NI6601_BUFF_COUNTER_RUNNING )
        return ni6601_errno = NI6601_ERR_IVA;

    if ( count == NULL )
        return ni6601_errno = NI6601_ERR_IVA;

    if ( wait_for_end &&
         ( dev_info[ board ].state[ counter ] == NI6601_CONT_PULSER_RUNNING ||
           dev_info[ board ].state[ counter ] == NI6601_CONT_COUNTER_RUNNING )
        )
        return ni6601_errno = NI6601_ERR_WFC;

    if ( state != NULL &&
         ( ret = ni6601_state( board, counter, state ) ) < 0 )
        return ret;

    v.counter = counter;
    v.wait_for_end = wait_for_end ? 1 : 0;
    v.do_poll = do_poll ? 1 : 0;

    if ( ioctl( dev_info[ board ].fd, NI6601_IOC_COUNT, &v ) < 0 )
        return ni6601_errno =
                          ( errno == EINTR ) ? NI6601_ERR_ITR : NI6601_ERR_INT;

    *count = v.count;

    if ( wait_for_end )
    {
        dev_info[ board ].state[ counter ] = NI6601_IDLE;       
        dev_info[ board ].state[ counter + ( counter & 1 ? -1 : 1 ) ]
                                                                 = NI6601_IDLE;
    }

    return ni6601_errno = NI6601_OK;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

int ni6601_generate_single_pulse( int    board,
                                  int    counter,
                                  double duration )
{
    int ret;
    NI6601_PULSES p;
    unsigned long len;
    int state;


    if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
        return ni6601_errno = NI6601_ERR_NSC;

    if ( dev_info[ board ].state[ counter ] != NI6601_IDLE )
        return ni6601_errno = NI6601_ERR_CBS;

    if ( ni6601_time_to_ticks( duration, &len ) < 0 )
        return ni6601_errno = NI6601_ERR_IVA;

    if ( ( ret = check_board( board ) ) < 0 )
        return ret; 

    if ( ( ret = ni6601_state( board, counter, &state ) ) < 0 )
        return ret;

    if ( state == NI6601_BUSY )
        return ni6601_errno = NI6601_ERR_CBS;

    p.counter = counter;
    p.continuous = 1;
    p.low_ticks = 2;
    p.high_ticks = len;
    p.source = NI6601_TIMEBASE_1;
    p.gate = NI6601_NONE;
    p.disable_output = 0;
    p.output_polarity = NI6601_NORMAL;

    if ( ioctl( dev_info[ board ].fd, NI6601_IOC_PULSER, &p ) < 0 )
        return ni6601_errno = NI6601_ERR_INT;

    dev_info[ board ].state[ counter ] = NI6601_PULSER_RUNNING;

    return ni6601_errno = NI6601_OK;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

int ni6601_generate_continuous_pulses( int    board,
                                       int    counter,
                                       double high_phase,
                                       double low_phase )
{
    int ret;
    int state;
    NI6601_PULSES p;
    unsigned long ht, lt;


    if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
        return ni6601_errno = NI6601_ERR_NSC;

    if ( ni6601_time_to_ticks( high_phase, &ht ) < 0 ||
         ni6601_time_to_ticks( low_phase, &lt ) < 0 )
        return ni6601_errno = NI6601_ERR_IVA;

    if ( ( ret = check_board( board ) ) < 0 )
        return ret; 

    if ( ( ret = ni6601_state( board, counter, &state ) ) < 0 )
        return ret;

    if ( state == NI6601_BUSY )
        return ni6601_errno = NI6601_ERR_CBS;

    p.counter = counter;
    p.continuous = 1;
    p.low_ticks = lt;
    p.high_ticks = ht;
    p.source = NI6601_TIMEBASE_1;
    p.gate = NI6601_NONE;
    p.disable_output = 0;
    p.output_polarity = NI6601_NORMAL;

    if ( ioctl( dev_info[ board ].fd, NI6601_IOC_PULSER, &p ) < 0 )
        return ni6601_errno = NI6601_ERR_INT;

    dev_info[ board ].state[ counter ] = NI6601_CONT_PULSER_RUNNING;

    return ni6601_errno = NI6601_OK;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

int ni6601_stop_pulses( int board,
                        int counter )
{
    return ni6601_stop_counter( board, counter );
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

int ni6601_dio_write( int           board,
                      unsigned char bits,
                      unsigned char mask )
{
    int ret;
    NI6601_DIO_VALUE dio;


    if ( ( ret = check_board( board ) ) < 0 )
        return ret;

    dio.value = bits;
    dio.mask = mask;

    if ( ioctl( dev_info[ board ].fd, NI6601_IOC_DIO_OUT, &dio ) < 0 )
        return ni6601_errno = NI6601_ERR_INT;

    return ni6601_errno = NI6601_OK;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

int ni6601_dio_read( int             board,
                     unsigned char * bits,
                     unsigned char   mask )
{
    int ret;
    NI6601_DIO_VALUE dio;


    if ( bits == NULL )
        return ni6601_errno = NI6601_ERR_IVA;

    if ( ( ret = check_board( board ) ) < 0 )
        return ret; 

    dio.mask = mask;
    if ( ioctl( dev_info[ board ].fd, NI6601_IOC_DIO_IN, &dio ) < 0 )
        return ni6601_errno = NI6601_ERR_INT;

    *bits = dio.value;

    return ni6601_errno = NI6601_OK;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

int ni6601_is_counter_armed( int   board,
                             int   counter,
                             int * state )
{
    int ret;


    if ( state == NULL )
        return ni6601_errno = NI6601_ERR_IVA;

    if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
        return ni6601_errno = NI6601_ERR_NSC;

    if ( ( ret = check_board( board ) ) < 0 )
        return ret; 

    return ni6601_is_armed( board, counter, state );
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

static int ni6601_is_armed( int   board,
                            int   counter,
                            int * state )
{
    NI6601_IS_ARMED a;


    a.counter = counter;

    if ( ioctl( dev_info[ board ].fd, NI6601_IOC_IS_BUSY, &a ) < 0 )
        return ni6601_errno = NI6601_ERR_INT;

    *state = a.state ? NI6601_BUSY : NI6601_IDLE;

    return ni6601_errno = NI6601_OK;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

static int ni6601_state( int   board,
                         int   counter,
                         int * state )
{
    if ( dev_info[ board ].state[ counter ] == NI6601_IDLE )
    {
        *state = NI6601_IDLE;
        return ni6601_errno = NI6601_OK;
    }

    if ( dev_info[ board ].state[ counter ] == NI6601_CONT_COUNTER_RUNNING ||
         dev_info[ board ].state[ counter ] == NI6601_BUFF_COUNTER_RUNNING ||
         dev_info[ board ].state[ counter ] == NI6601_CONT_PULSER_RUNNING )
    {
        *state = NI6601_BUSY;
        return ni6601_errno = NI6601_OK;
    }

    return ni6601_is_armed( board, counter, state );
}


/*--------------------------------------------------------------------*
 * This function gets called always before a board is really accessed
 * to determine if the board number is valid. If the board has never
 * been used before the function will try to open() the device file
 * for the board.
 * If opening the device file fails this indicates that there's no
 * board with the number passed to the function or that the board is
 * already in use by some other process and the function returns the
 * appropriate error number.
 * On success the boards file descriptor is opened and 0 is returned.
 *--------------------------------------------------------------------*/

static int check_board( int board )
{
    char name[ 20 ] = "/dev/" NI6601_DEVICE_NAME;
    int i;


    if ( board < 0 || board >= NI6601_MAX_BOARDS )
        return ni6601_errno = NI6601_ERR_NSB;

    if ( ! dev_info[ board ].is_init )
    {
        /* Cobble together the device file name */

        snprintf( name + strlen( name ), 20 - strlen( name ), "%d", board );

        /* Try to open the device file */

        if ( ( dev_info[ board ].fd = open( name, O_RDWR ) ) < 0 )
            switch ( errno )
            {
                case ENOENT :  case ENOTDIR : case ENAMETOOLONG : case ELOOP :
                    return ni6601_errno = NI6601_ERR_DFM;

                case EACCES :
                    return ni6601_errno = NI6601_ERR_ACS;

                case ENODEV : case ENXIO :
                    return ni6601_errno = NI6601_ERR_NDV;
            
                case EBUSY :
                    return ni6601_errno = NI6601_ERR_BBS;

                default :
                    return ni6601_errno = NI6601_ERR_DFP;
            }

        /* Set the FD_CLOEXEC bit for the device file - exec()'ed application
           should have no interest at all in the board (and even don't know
           that they have the file open) but could interfere seriously with
           normal operation of the program that did open the device file. */

        fcntl( dev_info[ board ].fd, F_SETFD, FD_CLOEXEC );

        dev_info[ board ].is_init = 1;

        for ( i = NI6601_COUNTER_0; i <= NI6601_COUNTER_0; i++ )
            dev_info[ board ].state[ i ] = NI6601_IDLE;
    }

    return ni6601_errno = NI6601_OK;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

static int check_source( int source )
{
    if ( source == NI6601_LOW || 
         source == NI6601_DEFAULT_SOURCE ||
         source == NI6601_SOURCE_0 ||
         source == NI6601_SOURCE_1 ||
         source == NI6601_SOURCE_2 ||
         source == NI6601_SOURCE_3 ||
         source == NI6601_NEXT_GATE ||
         source == NI6601_TIMEBASE_1 ||
         source == NI6601_TIMEBASE_2 ||
         source == NI6601_TIMEBASE_3 )
        return ni6601_errno = NI6601_OK;

    return ni6601_errno = NI6601_ERR_IVS;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

static int ni6601_time_to_ticks( double          time,
                                 unsigned long * ticks )
{
    unsigned long t;


    error_message = "";

    if ( time <= 0.0 )
        return ni6601_errno = NI6601_ERR_IVA;

    if ( time / NI6601_TIME_RESOLUTION - 1 > ULONG_MAX )
        return ni6601_errno = NI6601_ERR_IVA;

    if ( ( t = ( unsigned long )
                          floor( time / NI6601_TIME_RESOLUTION + 0.5 ) ) == 0 )
        return ni6601_errno = NI6601_ERR_IVA;

    *ticks = t;

    return ni6601_errno = NI6601_OK;
}


/*---------------------------------------------------------------*
 * Prints out a string to stderr, consisting of a user supplied
 * string (the argument of the function), a colon, a blank and
 * a short descriptive text of the error encountered in the last
 * invocation of one of the ni6601_xxx() functions, followed by
 * a new-line. If the argument is NULL or an empty string only
 * the error message is printed.
 *---------------------------------------------------------------*/

int ni6601_perror( const char * s )
{
    if ( s != NULL && *s != '\0' )
        return fprintf( stderr, "%s: %s\n",
                        s, ni6601_errlist[ - ni6601_errno ] );

    return fprintf( stderr, "%s\n", ni6601_errlist[ - ni6601_errno ] );
}


/*---------------------------------------------------------------*
 * Returns a string with a short descriptive text of the error
 * encountered in the last invocation of one of the ni6601_xxx()
 * functions.
 *---------------------------------------------------------------*/

const char *ni6601_strerror( void )
{
    return ni6601_errlist[ - ni6601_errno ];
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
