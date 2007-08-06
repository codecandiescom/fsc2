/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2007 Jens Thoms Toerring
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


#include "pci_mio_16e_1.h"


#define COUNTER_IS_BUSY 1

static NI_DAQ_INPUT pci_mio_16e_1_gpct_source( const char * tname,
                                               const char * snippet );


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *daq_start_continuous_counter( Var_T * v )
{
    long counter;
    NI_DAQ_INPUT source;
    int ret;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    counter = pci_mio_16e_1_channel_number( get_strict_long( v, "counter" ),
                                            "counter" );

    if ( counter > 2 )
    {
        print( FATAL, "Invalid counter number %ld.\n", counter );
        THROW( EXCEPTION );
    }

    if ( ( v = vars_pop( v ) ) == NULL || v->type != STR_VAR )
    {
        print( FATAL, "Missing source for counting.\n" );
        THROW( EXCEPTION );
    }

    source = pci_mio_16e_1_gpct_source( v->val.sptr, "counter source" );

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni_daq_gpct_start_counter( pci_mio_16e_1.board,
                                         ( int ) counter, source );
        lower_permissions( );

        switch( ret )
        {
            case NI_DAQ_OK :
                break;

            case NI_DAQ_ERR_CBS :
                print( FATAL, "Counter CH%ld is already running.\n", counter );
                THROW( EXCEPTION );

            default :
                print( FATAL, "Can't start counter CH%ld.\n", counter );
                THROW( EXCEPTION );
        }
    }
    else if ( FSC2_MODE == TEST )
    {
        if ( pci_mio_16e_1.gpct_state.states[ counter ] == COUNTER_IS_BUSY )
        {
            print( FATAL, "Counter CH%d is already running.\n", counter );
            THROW( EXCEPTION );
        }

        pci_mio_16e_1.gpct_state.states[ counter ] = COUNTER_IS_BUSY;
    }

    return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *daq_start_timed_counter( Var_T * v )
{
    int counter;
    NI_DAQ_INPUT source;
    double interval;
    int ret;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    counter = pci_mio_16e_1_channel_number( get_strict_long( v, "counter" ),
                                     "counter" );

    if ( counter > 2 )
    {
        print( FATAL, "Invalid counter number %ld.\n", counter );
        THROW( EXCEPTION );
    }

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing time interval for counting.\n" );
        THROW( EXCEPTION );
    }

    interval = pci_mio_16e_1_check_time( get_double( v,
                                                    "counting time interval" ),
                                         "counting time interval" );

    if ( ( v = vars_pop( v ) ) == NULL || v->type != STR_VAR )
    {
        print( FATAL, "Missing source for counting.\n" );
        THROW( EXCEPTION );
    }

    source = pci_mio_16e_1_gpct_source( v->val.sptr, "counter source" );

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni_daq_gpct_start_gated_counter( pci_mio_16e_1.board, counter,
                                               interval, source );
        lower_permissions( );

        switch ( ret )
        {
            case NI_DAQ_OK :
                break;

            case NI_DAQ_ERR_CBS :
                print( FATAL, "Counter CH%d is already running.\n", counter );
                THROW( EXCEPTION );

            case NI_DAQ_ERR_NCB :
                print( FATAL, "Required neighbouring counter CH%d is already "
                       "running.\n", counter & 1 ? counter - 1 : counter + 1 );
                THROW( EXCEPTION );

            case NI_DAQ_ERR_NPT :
                print( FATAL, "Impossible to use the requested timings.\n" );
                break;

            default :
                print( FATAL, "Can't start counter.\n" );
                THROW( EXCEPTION );
        }
    }
    else if (    FSC2_MODE == TEST
              && pci_mio_16e_1.gpct_state.states[ counter ]
                                                           == COUNTER_IS_BUSY )
    {
        print( FATAL, "Counter CH%d is already running.\n", counter );
        THROW( EXCEPTION );
    }

    return vars_push( INT_VAR, 1 );
}


/*-----------------------------------------------------------------*
 * This function is a combination of counter_start_timed_counter()
 * and counter_final_count() - it starts a timed count and waits
 * for the count interval finish and then fetches the count.
 *-----------------------------------------------------------------*/

Var_T *daq_timed_count( Var_T * v )
{
    int counter;
    NI_DAQ_INPUT source;
    double interval;
    unsigned long count;
    int state;
    static long dummy_count = 0;
    int ret;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    counter = pci_mio_16e_1_channel_number( get_strict_long( v, "counter" ),
                                     "counter" );

    if ( counter > 2 )
    {
        print( FATAL, "Invalid counter number %ld.\n", counter );
        THROW( EXCEPTION );
    }

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing time interval for counting.\n" );
        THROW( EXCEPTION );
    }

    interval = pci_mio_16e_1_check_time( get_double( v,
                                                    "counting time interval" ),
                                         "counting time interval" );

    if ( ( v = vars_pop( v ) ) == NULL || v->type != STR_VAR )
    {
        print( FATAL, "Missing source for counting.\n" );
        THROW( EXCEPTION );
    }

    source = pci_mio_16e_1_gpct_source( v->val.sptr, "source channel" );
    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni_daq_gpct_start_gated_counter( pci_mio_16e_1.board, counter,
                                               interval, source );
        lower_permissions( );

        switch ( ret )
        {
            case NI_DAQ_OK :
                break;

            case NI_DAQ_ERR_CBS :
                print( FATAL, "Counter CH%d is already running.\n", counter );
                THROW( EXCEPTION );

            case NI_DAQ_ERR_NCB :
                print( FATAL, "Required neighbouring counter CH%d is already "
                       "running.\n", counter & 1 ? counter - 1 : counter + 1 );
                THROW( EXCEPTION );

            case NI_DAQ_ERR_NPT :
                print( FATAL, "Impossible to use the requested timings.\n" );
                break;

            default :
                print( FATAL, "Can't start counter.\n" );
                THROW( EXCEPTION );
        }
    }
    else if (    FSC2_MODE == TEST
              && pci_mio_16e_1.gpct_state.states[ counter ]
                                                           == COUNTER_IS_BUSY )
    {
        print( FATAL, "Counter CH%d is already running.\n", counter );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
    {
        /* For longer intervals (i.e. longer than the typical time resolution
           of the machine) sleep instead of waiting in the kernel for the
           count interval to finish (as we would do when calling
           ni6601_get_count() immediately with the third argument being set
           to 1). If the user pressed the "Stop" button while we were
           sleeping stop the counter and return the current count without
           further waiting. */

        if ( ( interval -= 0.01 ) > 0.0 )
        {
            fsc2_usleep( ( unsigned long ) ( interval * 1.0e6 ), SET );
            if ( check_user_request( ) )
                daq_stop_counter( vars_push( INT_VAR, counter ) );
        }

    try_counter_again:

        raise_permissions( );
        ret = ni_daq_gpct_get_count( pci_mio_16e_1.board, counter, 1,
                                     &count, &state );
        lower_permissions( );

        switch ( ret )
        {
            case NI_DAQ_OK :
                if ( count > LONG_MAX )
                {
                    print( SEVERE, "Counter value too large.\n" );
                    count = LONG_MAX;
                }
                break;

            case NI_DAQ_ERR_WFC :
                print( FATAL, "Can't get final count, counter CH%d is "
                       "running in continuous mode.\n", counter );
                THROW( EXCEPTION );

            case NI_DAQ_ERR_ITR :
                goto try_counter_again;

            default :
                print( FATAL, "Can't get counter value.\n" );
                THROW( EXCEPTION );
        }
    }
    else
        count = ++dummy_count;

    return vars_push( INT_VAR, ( long ) count );
}


/*------------------------------------------------------*
 * Gets a count even while the counter is still running
 *------------------------------------------------------*/

Var_T *daq_intermediate_count( Var_T * v )
{
    int counter;
    unsigned long count;
    int state;
    static long dummy_count = 0;
    int ret;


    counter = pci_mio_16e_1_channel_number( get_strict_long( v,
                                                           "counter channel" ),
                                            "counter channel" );

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni_daq_gpct_get_count( pci_mio_16e_1.board, counter, 0,
                                     &count, &state );
        lower_permissions( );

        if ( ret < 0 )
        {
            print( FATAL, "Can't get counter value.\n" );
            THROW( EXCEPTION );
        }

        if ( count > LONG_MAX )
        {
            print( SEVERE, "Counter value too large.\n" );
            count = LONG_MAX;
        }
    }
    else
        count = ++dummy_count;

    return vars_push( INT_VAR, count );
}


/*------------------------------------------------------------------*
 * Get a count after waiting until the counter is finished counting
 *------------------------------------------------------------------*/

Var_T *daq_final_count( Var_T * v )
{
    int counter;
    unsigned long count;
    int state;
    static long dummy_count = 0;
    int ret;


    counter = pci_mio_16e_1_channel_number( get_strict_long( v,
                                                           "counter channel" ),
                                            "counter channel" );

    if ( FSC2_MODE == EXPERIMENT )
    {

    try_counter_again:

        raise_permissions( );
        ret = ni_daq_gpct_get_count( pci_mio_16e_1.board, counter, 1,
                                     &count, &state );
        lower_permissions( );

        switch ( ret )
        {
            case NI_DAQ_OK :
                if ( count > LONG_MAX )
                {
                    print( SEVERE, "Counter value too large.\n" );
                    count = LONG_MAX;
                }
                break;

            case NI_DAQ_ERR_WFC :
                print( FATAL, "Can't get final count, counter CH%d is "
                       "running in continuous mode.\n", counter );
                THROW( EXCEPTION );

            case NI_DAQ_ERR_ITR :
                goto try_counter_again;

            default :
                print( FATAL, "Can't get counter value.\n" );
                THROW( EXCEPTION );
        }
    }
    else
        count = ++dummy_count;

    return vars_push( INT_VAR, ( long ) count );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *daq_stop_counter( Var_T * v )
{
    int counter;
    int ret;


    counter = pci_mio_16e_1_channel_number( get_strict_long( v,
                                                           "counter channel" ),
                                            "counter channel" );

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni_daq_gpct_stop_counter( pci_mio_16e_1.board, counter );
        lower_permissions( );

        if ( ret < 0 )
            print( SEVERE, "Failed to stop counter CH%d.\n", counter );
    }
    else if ( FSC2_MODE == TEST )
        pci_mio_16e_1.gpct_state.states[ counter ] = 0;

    return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *daq_single_pulse( Var_T * v )
{
    int counter;
    double duration;
    double dummy = 0.0;
    int ret;


    counter = pci_mio_16e_1_channel_number( get_strict_long( v,
                                                           "counter channel" ),
                                            "counter channel" );
    duration = pci_mio_16e_1_check_time( get_double( v->next, "pulse length" ),
                                         "pulse length" );

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni_daq_gpct_single_pulse( pci_mio_16e_1.board, counter,
                                        duration, &dummy, 0 );
        lower_permissions( );

        switch ( ret )
        {
            case NI_DAQ_OK :
                break;

            case NI_DAQ_ERR_CBS :
                print( FATAL, "Counter CH%d requested for pulse is already "
                       "running.\n", counter );
                THROW( EXCEPTION );

            default :
                print( FATAL, "Can't create the pulse.\n" );
                THROW( EXCEPTION );
        }
    }
    else if (    FSC2_MODE == TEST
              && pci_mio_16e_1.gpct_state.states[ counter ]
                                                           == COUNTER_IS_BUSY )
    {
        print( FATAL, "Counter CH%d requested for pulse is already "
               "running.\n", counter );
        THROW( EXCEPTION );
    }

    return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *daq_continuous_pulses( Var_T * v )
{
    int counter;
    double len_hi, len_low;
    double dummy = 0.0;
    int ret;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    counter = pci_mio_16e_1_channel_number( get_strict_long( v,
                                                           "counter channel" ),
                                            "counter channel" );

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing pulse length.\n" );
        THROW( EXCEPTION );
    }

    len_hi = pci_mio_16e_1_check_time( get_double( v,
                                              v->next != NULL ?
                                              "length of high phase of pulse" :
                                              "pulse length" ),
                                       v->next != NULL ?
                                       "length of high phase of pulse" :
                                       "pulse length" );

    if ( ( v = vars_pop( v ) ) != NULL )
        len_low = pci_mio_16e_1_check_time( get_double( v,
                                              "length of low phase of pulse" ),
                                            "length of low phase of pulse" );
    else
        len_low = len_hi;

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni_daq_gpct_continuous_pulses( pci_mio_16e_1.board, counter,
                                             len_hi, len_low, &dummy, 0 );
        lower_permissions( );

        switch ( ret )
        {
            case NI_DAQ_OK :
                break;

            case NI_DAQ_ERR_CBS :
                print( FATAL, "Counter CH%d requested for continnuous pulses "
                       "is already running.\n", counter );
                THROW( EXCEPTION );

            default :
                print( FATAL, "Can't create continuous pulses.\n" );
                THROW( EXCEPTION );
        }
    }
    else if (    FSC2_MODE == TEST
              && pci_mio_16e_1.gpct_state.states[ counter ]
                                                           == COUNTER_IS_BUSY )
    {
        print( FATAL, "Counter CH%d requested for continuous pulses is "
               "already running.\n", counter );
        THROW( EXCEPTION );
    }

    return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static NI_DAQ_INPUT pci_mio_16e_1_gpct_source( const char * tname,
                                               const char * snippet )
{
    NI_DAQ_INPUT pfi = NI_DAQ_PFI0;
    int i;


    if ( ! strncasecmp( "PFI", tname, 3 ) )
        for ( i = 0; i < 10; i++ )
            if ( tname[ 3 ] == '0' + i  )
                return pfi + i;

    if ( ! strcasecmp( "TIMEBASE_1", tname ) )
        return NI_DAQ_IN_TIMEBASE1;

    if ( ! strcasecmp( "TIMEBASE_2", tname ) )
        return NI_DAQ_IN_TIMEBASE2;

    if ( ! strcasecmp( "TC_OTHER", tname ) )
        return NI_DAQ_SOURCE_TC_OTHER;

    print( FATAL, "Invalid source for %s.\n", snippet );
    THROW( EXCEPTION );

    return -1;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void ni_daq_two_channel_pulses( double delay,
                                double scan_duration )
{
    double len_hi_0, len_hi_1,
           len_low,
           len_del = 0.0;


    len_hi_1 = pci_mio_16e_1_check_time( PCI_MIO_16E_1_TRIGGER_LENGTH,
                                         "trigger length" );

    if ( FSC2_MODE == TEST )
    {
        if (    pci_mio_16e_1.gpct_state.states[ 0 ] == COUNTER_IS_BUSY
             || pci_mio_16e_1.gpct_state.states[ 1 ] == COUNTER_IS_BUSY )
        {
            print( FATAL, "Required counter is already in use.\n" );
            THROW( EXCEPTION );
        }

        return;
    }

    len_hi_0 = 4.0e-7;
    len_low = scan_duration - len_hi_0;

    if ( delay >= 0.0 )
    {
        switch ( ni_daq_gpct_continuous_pulses( pci_mio_16e_1.board, 0,
                                                len_hi_0, len_low,
                                                &len_del, 1 ) )
        {
            case NI_DAQ_OK :
                break;

            case NI_DAQ_ERR_CBS :
                print( FATAL, "Required counter is already in use.\n" );
                THROW( EXCEPTION );

            default :
                print( FATAL, "Can't create output trigger.%s\n",
                       ni_daq_strerror() );
                THROW( EXCEPTION );
        }

        len_del += delay;

        switch ( ni_daq_gpct_single_pulse( pci_mio_16e_1.board, 1,
                                           len_hi_1, &len_del, 1 ) )
        {
            case NI_DAQ_OK :
                break;

            case NI_DAQ_ERR_CBS :
                print( FATAL, "Required counter is already in use.\n" );
                THROW( EXCEPTION );

            default :
                print( FATAL, "Can't create output trigger.%s\n",
                       ni_daq_strerror());
                THROW( EXCEPTION );
        }
    }
    else
    {
        switch ( ni_daq_gpct_single_pulse( pci_mio_16e_1.board, 1,
                                           len_hi_1, &len_del, 1 ) )
        {
            case NI_DAQ_OK :
                break;

            case NI_DAQ_ERR_CBS :
                print( FATAL, "Required counter is already in use.\n" );
                THROW( EXCEPTION );

            default :
                print( FATAL, "Can't create output trigger.%s\n",
                       ni_daq_strerror() );
                THROW( EXCEPTION );
        }

        len_del = fabs( delay + len_del );

        switch ( ni_daq_gpct_continuous_pulses( pci_mio_16e_1.board, 0,
                                                len_hi_0, len_low,
                                                &len_del, 1 ) )
        {
            case NI_DAQ_OK :
                break;

            case NI_DAQ_ERR_CBS :
                print( FATAL, "Required counter is already in use.\n" );
                THROW( EXCEPTION );

            default :
                print( FATAL, "Can't create output trigger.%s\n",
                       ni_daq_strerror() );
                THROW( EXCEPTION );
        }
    }
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
