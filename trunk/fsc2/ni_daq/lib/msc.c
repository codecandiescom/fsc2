/*
 *  $Id$
 * 
 *  Library for National Instruments DAQ boards based on a DAQ-STC
 * 
 *  Copyright (C) 2003-2006 Jens Thoms Toerring
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 * 
 *  To contact the author send email to:  jt@toerring.de
 */


#include "ni_daq_lib.h"


/*----------------------------------------------------------------*
 * Function for setting the clock speeds - the 'speed' argument
 * tells if the frequency output of both clocks is to be reduced
 * by a factor of 2 (as seen on the FREQ_OUT pin and by the sub-
 * systems of the boards for IN_TIMEBASE2), the divider (between
 * 1 and 16) gets set for both clocks and applies to what appears
 * on the FREQ_OUT pin only.
 *----------------------------------------------------------------*/

int ni_daq_msc_set_clock_speed( int                      board,
                                NI_DAQ_CLOCK_SPEED_VALUE speed,
                                int                      divider )
{
    NI_DAQ_MSC_ARG msc;
    int ret;


    if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
        return ret;

    if ( ( speed != NI_DAQ_FULL_SPEED && speed != NI_DAQ_HALF_SPEED ) ||
         divider < 1 || divider > 16 )
        return ni_daq_errno = NI_DAQ_ERR_IVA;

    msc.cmd = NI_DAQ_MSC_SET_CLOCK_SPEED;
    msc.speed = speed;
    msc.divider = divider;

    if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_MSC, &msc ) < 0 )
        return ni_daq_errno = NI_DAQ_ERR_INT;

    ni_daq_dev[ board ].msc_state.speed = speed;
    ni_daq_dev[ board ].msc_state.divider = divider;

    return ni_daq_errno = NI_DAQ_OK;
}


/*-----------------------------------------------------------------*
 * Function for setting the clock to be output at the FREQ_OUT pin
 * and for enabling and disabling the frequency output
 *-----------------------------------------------------------------*/

int ni_daq_msc_set_clock_output( int               board,
                                 NI_DAQ_CLOCK_TYPE daq_clock,
                                 NI_DAQ_STATE      on_off )
{
    NI_DAQ_MSC_ARG msc;
    int ret;


    if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
        return ret;

    if ( ( daq_clock != NI_DAQ_FAST_CLOCK &&
           daq_clock != NI_DAQ_SLOW_CLOCK ) ||
         ( on_off != NI_DAQ_ENABLED && on_off != NI_DAQ_DISABLED ) )
        return ni_daq_errno = NI_DAQ_ERR_IVA;

    msc.cmd = NI_DAQ_MSC_CLOCK_OUTPUT;
    msc.clock = daq_clock;
    msc.output_state = on_off;

    if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_MSC, &msc ) < 0 )
        return ni_daq_errno = NI_DAQ_ERR_INT;

    ni_daq_dev[ board ].msc_state.clock = daq_clock;
    ni_daq_dev[ board ].msc_state.output_state = on_off;

    return ni_daq_errno = NI_DAQ_OK;
}


/*------------------------------------------------------------------*
 * Function for determining all current clock settings of the board
 *------------------------------------------------------------------*/

int ni_daq_msc_get_clock_state( int                        board,
                                NI_DAQ_CLOCK_TYPE *        daq_clock,
                                NI_DAQ_STATE *             on_off,
                                NI_DAQ_CLOCK_SPEED_VALUE * speed,
                                int *                      divider )
{
    int ret;


    if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
        return ret;

    if ( daq_clock == NULL || on_off == NULL ||
         speed == NULL || divider == NULL )
        return ni_daq_errno = NI_DAQ_ERR_IVA;

    *daq_clock = ni_daq_dev[ board ].msc_state.clock;
    *on_off = ni_daq_dev[ board ].msc_state.output_state;
    *speed = ni_daq_dev[ board ].msc_state.speed;
    *divider = ni_daq_dev[ board ].msc_state.divider;

    return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

int ni_daq_msc_set_trigger( int              board,
                            NI_DAQ_TRIG_TYPE trigger_type,
                            double           trigger_high,
                            double           trigger_low )
{
    NI_DAQ_MSC_ARG a;
    int ret;


    if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
        return ret;

    if ( trigger_type != NI_DAQ_TRIG_TTL )
    {
        unsigned int max_trig;

        if ( ! ni_daq_dev[ board ].props.has_analog_trig )
            return ni_daq_errno = NI_DAQ_ERR_NAT;

        if ( trigger_type == NI_DAQ_TRIG_LOW_WINDOW )
            trigger_high = 10.0;
        else if ( trigger_type == NI_DAQ_TRIG_HIGH_WINDOW )
            trigger_low = -10.0;

        if ( trigger_low >= trigger_high ||
             trigger_high >= 10.001 ||
             trigger_low <= -10.001 )
            return ni_daq_errno = NI_DAQ_ERR_IVA;

        max_trig = 1 << ni_daq_dev[ board ].props.atrig_bits;
        
        a.trigger_low = ( int ) floor( 0.05 * ( trigger_low + 10.0 )
                                       * max_trig  + 0.5 ) - max_trig / 2;
        a.trigger_high = ( int ) floor( 0.05 * ( trigger_high + 10.0 )
                                        * max_trig + 0.5 ) - max_trig / 2 - 1;
    }

    a.cmd = NI_DAQ_MSC_TRIGGER_STATE;
    a.trigger_type = trigger_type;

    if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_MSC, &a ) < 0 )
        return ni_daq_errno = NI_DAQ_ERR_INT;

    return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------*
 * Function is used only internally: when the board is opened it gets
 * called to find out the current clock settings of the board
 *--------------------------------------------------------------------*/

int ni_daq_msc_init( int board )
{
    int ret;
    NI_DAQ_MSC_ARG msc;

    msc.cmd = NI_DAQ_MSC_GET_CLOCK;

    if ( ( ret == ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_MSC, &msc ) ) < 0 )
        return 1;

    msc.cmd = NI_DAQ_MSC_TRIGGER_STATE;
    msc.trigger_type = NI_DAQ_TRIG_TTL;

    if ( ( ret == ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_MSC, &msc ) ) < 0 )
        return 1;

    ni_daq_dev[ board ].msc_state.clock = msc.clock;
    ni_daq_dev[ board ].msc_state.output_state = msc.output_state;
    ni_daq_dev[ board ].msc_state.speed = msc.speed;
    ni_daq_dev[ board ].msc_state.divider = msc.divider;

    return 0;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
