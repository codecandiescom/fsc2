/*
 *  Library for National Instruments DAQ boards based on a DAQ-STC
 * 
 *  Copyright (C) 2003-2009 Jens Thoms Toerring
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


static int ni_daq_acq_start_check( NI_DAQ_INPUT /* as */ );

static int ni_daq_scan_start_check( NI_DAQ_INPUT /* ss */ );

static int ni_daq_conv_start_check( NI_DAQ_INPUT /* cs */ );

static int ni_daq_get_ai_timings( int             /* board */,
                                  NI_DAQ_INPUT *  /* ss    */,
                                  double          /* sd    */,
                                  unsigned long * /* sl    */,
                                  NI_DAQ_INPUT *  /* cs    */,
                                  double          /* cd    */,
                                  unsigned long * /* cl    */ );

static int ni_daq_get_ai_timings2( int             /* board */,
                                   NI_DAQ_INPUT *  /* ss    */,
                                   double          /* sd    */,
                                   unsigned long * /* sl    */,
                                   NI_DAQ_INPUT *  /* cs    */,
                                   double          /* cd    */,
                                   unsigned long * /* cl    */ );


/*---------------------------------------------------------------------*
 * Function for switching the AI_IN_TIMEBASE1 (i.e. the fast time base)
 * between 20 MHz and 10 MHz
 *---------------------------------------------------------------------*/

int ni_daq_ai_set_speed( int                      board,
                         NI_DAQ_CLOCK_SPEED_VALUE speed )
{
    NI_DAQ_AI_ARG a;
    int ret;


    if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
        return ret;

    if ( speed != NI_DAQ_FULL_SPEED && speed != NI_DAQ_HALF_SPEED )
        return ni_daq_errno = NI_DAQ_ERR_IVA;

    if ( ni_daq_dev[ board ].gpct_state.speed == speed )
        return ni_daq_errno = NI_DAQ_OK;

    a.cmd = NI_DAQ_GPCT_SET_CLOCK_SPEED;
    a.speed = speed;

    if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_AI, &a ) < 0 )
        return ni_daq_errno = NI_DAQ_ERR_INT;
    
    ni_daq_dev[ board ].ai_state.speed = speed;

    return ni_daq_errno = NI_DAQ_OK;
}


/*-------------------------------------------------------------------*
 * Function to determine if AI_IN_TIMEBASE1 (i.e. the fast time base)
 * is running at 20 MHz or at 10 MHz
 *-------------------------------------------------------------------*/

int ni_daq_ai_get_speed( int                        board,
                         NI_DAQ_CLOCK_SPEED_VALUE * speed )
{
    int ret;


    if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
        return ret;

    if ( speed == NULL )
        return ni_daq_errno = NI_DAQ_ERR_IVA;

    *speed = ni_daq_dev[ board ].ai_state.speed;

    return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

int ni_daq_ai_channel_configuration( int                  board,
                                     int                  num_channels,
                                     int *                channels,
                                     NI_DAQ_AI_TYPE *     types,
                                     NI_DAQ_BU_POLARITY * polarities,
                                     double *             ranges,
                                     NI_DAQ_STATE *       dither_enables )
{
    int ret;
    int gi;
    NI_DAQ_AI_ARG a;
    NI_DAQ_AI_CHANNEL_ARGS *cargs;
    int i, j;
    int num_data_channels = 0;
    

    /* Very basic checks... */

    if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
        return ret;

    ni_daq_dev[ board ].ai_state.num_channels = 0;
    ni_daq_dev[ board ].ai_state.num_data_per_scan = 0;

    if ( channels == NULL || types == NULL || polarities == NULL ||
         ranges == NULL || dither_enables == NULL )
        return ni_daq_errno = NI_DAQ_ERR_IVA;

    if ( num_channels < 1 )
        return ni_daq_errno = NI_DAQ_ERR_IVA;

    /* Populate a structure with the channels parameters */

    if ( ( cargs = malloc( num_channels * sizeof *cargs ) ) == NULL )
        return ni_daq_errno = NI_DAQ_ERR_NEM;

    if ( ni_daq_dev[ board ].ai_state.channels != NULL )
        free( ni_daq_dev[ board ].ai_state.channels );

    if ( ( ni_daq_dev[ board ].ai_state.channels =
           malloc( num_channels *
                   sizeof *ni_daq_dev[ board ].ai_state.channels ) ) == NULL )
    {
        free( cargs );
        return ni_daq_errno = NI_DAQ_ERR_NEM;
    }

    for ( i = 0; i < num_channels; i++ )
    {
        if ( channels[ i ] < 0 ||
             channels[ i ] >= ni_daq_dev[ board ].props.num_ai_channels )
            break;

        cargs[ i ].channel = channels[ i ];
        cargs[ i ].generate_trigger = NI_DAQ_DISABLED;

        if ( types[ i ] < 0 || types[ i ] > NI_DAQ_AI_TYPE_Ghost ||
             types[ i ] == 4 || types[ i ] == 6 )
            break;

        cargs[ i ].channel_type = types[ i ];

        if ( polarities[ i ] != NI_DAQ_BIPOLAR &&
             polarities[ i ] != NI_DAQ_UNIPOLAR )
            break;

        cargs[ i ].polarity = polarities[ i ];

        if ( ranges[ i ] <= 0.0 )
            break;

        gi = polarities[ i ] == NI_DAQ_BIPOLAR ? 0 : 1;

        for ( j = 0; j < 8; j++ )
        {
            if ( ni_daq_dev[ board ].props.ai_mV_ranges[ gi ][ j ] <= 0 )
                continue;

            if ( fabs( ni_daq_dev[ board ].props.ai_mV_ranges[ gi ][ j ]
                       - 1000 * ranges[ i ] ) <= 0.01 * ranges[ i ] )
            {
                cargs[ i ].gain = ( NI_DAQ_AI_GAIN_TYPES ) j;
                break;
            }

        }

        if ( j == 8 )
            break;

        cargs[ i ].dither_enable =
                        dither_enables[ i ] ? NI_DAQ_ENABLED : NI_DAQ_DISABLED;

        /* We've got to remember what the ranges and polarities the channels
         * returning data are set to to be able to do a conversion to volts */

        if ( types[ i ] == NI_DAQ_AI_TYPE_Ghost )
            continue;

        ni_daq_dev[ board ].ai_state.channels[ num_data_channels ].range =
                     0.001 * ni_daq_dev[ board ].props.ai_mV_ranges[ gi ][ j ];
        ni_daq_dev[ board ].ai_state.channels[ num_data_channels++ ].pol =
                                                               polarities[ i ];
    }

    if ( i < num_channels )
    {
        free( cargs );
        free( ni_daq_dev[ board ].ai_state.channels );
        ni_daq_dev[ board ].ai_state.channels = NULL;
        return ni_daq_errno = NI_DAQ_ERR_IVA;
    }

    /* Send it all to the board */

    a.cmd = NI_DAQ_AI_CHANNEL_SETUP;
    a.num_channels = num_channels;
    a.channel_args = cargs;

    if ( ( ret = ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_AI, &a ) ) < 0 )
    {
        free( cargs );
        free( ni_daq_dev[ board ].ai_state.channels );
        ni_daq_dev[ board ].ai_state.channels = NULL;
        return ni_daq_errno = NI_DAQ_ERR_INT;
    }

    free( cargs );

    if ( ret != num_data_channels )
    {
        free( ni_daq_dev[ board ].ai_state.channels );
        ni_daq_dev[ board ].ai_state.channels = NULL;
        return ni_daq_errno = NI_DAQ_ERR_INT;
    }

    ni_daq_dev[ board ].ai_state.num_channels = num_channels;
    ni_daq_dev[ board ].ai_state.num_data_per_scan = ret;

    return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------------*
 * Function for setting up (and possibly starting) an acquisition. The
 * 'start' argument specifies how (and when) the acquisition is to be
 * started. It can have the following values:
 *
 *   1. NI_DAQ_INTERNAL: acquisition is started by either calling
 *                       ni_daq_ai_start_acq() or by requesting data by
 *                       calling ni_daq_ai_get_acq_data()
 *   2. NI_DAQ_NOW:      acquisition is started immediately when the
 *                       the setup is done successfully
 *   3. NI_DAQ_PFI0 - NI_DAQ_PFI9: acquisition is started by a trigger
 *                       on one of the PFI0 to PFI9 input lines
 *   4. NI_DAQ_NI_DAQ_RTSI_0 - NI_DAQ_NI_DAQ_RTSI_6: acquisition is
 *                       started by a trigger on one of the RTSI inputs
 *   5. NI_DAQ_GOUT_0:   acquisition is started on a trigger from the
 *                       output of counter 0
 *
 * The 'start_polarity' is only relevant if 'start' isn't NI_DAQ_INTERNAL
 * or NI_DAQ_NOW and then selects the polarity of the trigger:
 *
 *   1. NI_DAQ_NORMAL:   trigger start of acquisition on raising edge
 *   2. NI_DAQ_INVERTED: trigger start of acquisition on falling edge
 * 
 * The 'scan_start' argument determines how scans are started. It can be
 *
 *   1. NI_DAQ_INTERNAL: Scans are started automatically with the time
 *                       between scans given by the 'scan_duration' argument
 *   2. NI_DAQ_PFI0 - NI_DAQ_PFI9: scans are started by a trigger on one of
 *                       the PFI0 to PFI8 input lines
 *   3. NI_DAQ_NI_DAQ_RTSI_0 - NI_DAQ_NI_DAQ_RTSI_6: scans are started on
 *                       triggers one of the RTSI inputs
 *   5. NI_DAQ_GOUT_0:   scans are started by a trigger from the output of
 *                       counter 0
 *
 * The 'scan_polarity' is only relevant if 'scan_start' isn't NI_DAQ_INTERNAL
 * and then selects the polarity of the trigger:
 *
 *   1. NI_DAQ_NORMAL:   trigger new scans on raising edge
 *   2. NI_DAQ_INVERTED: trigger new scans on falling edge
 * 
 * The 'scan_duration' argument is only used when 'scan_start' is set to
 * NI_DAQ_INTERNAL. In this case it is used to determine the time between
 * scans. Please note that the minimum time between scans must be at least
 * the minimum time resolution of the board and, when more than one channel
 * is sampled, the product of the number of channels and the minimum time
 * resolution of the board.
 *
 * The 'conv_start' argument determines how conversions are started. It can be
 *
 *   1. NI_DAQ_INTERNAL: Scans are conversion automatically with the time
 *                       between conversion given by the 'conv_duration'
 *                       argument
 *   2. NI_DAQ_PFI0 - NI_DAQ_PFI9: conversions are started by a trigger on
 *                       one of the PFI0 to PFI8 input lines
 *   3. NI_DAQ_NI_DAQ_RTSI_0 - NI_DAQ_NI_DAQ_RTSI_6: conversion are started on
 *                       triggers one of the RTSI inputs
 *   5. NI_DAQ_GOUT_0:   conversion are started by a trigger from the
 *                       output of counter 0
 *
 * The 'conv_polarity' is only relevant if 'conv_start' isn't NI_DAQ_INTERNAL
 * and then selects the polarity of the trigger:
 *
 *   1. NI_DAQ_NORMAL:   trigger new conversions on raising edge
 *   2. NI_DAQ_INVERTED: trigger new conversions on falling edge
 * 
 * The 'conv_duration' argument is only used when 'conv_start' is set to
 * NI_DAQ_INTERNAL. In this case it is used to determine the time between
 * conversions. Please note that the minimum time between conversions must
 * be at least the minimum time resolution of the board.
 *
 * The 'num_scans' argument determines how many scan are to be done. It can
 * be number between 1 and 2^24.
 *--------------------------------------------------------------------------*/

int ni_daq_ai_acq_setup( int             board,
                         NI_DAQ_INPUT    start,
                         NI_DAQ_POLARITY start_polarity,
                         NI_DAQ_INPUT    scan_start,
                         NI_DAQ_POLARITY scan_polarity,
                         double          scan_duration,
                         NI_DAQ_INPUT    conv_start,
                         NI_DAQ_POLARITY conv_polarity,
                         double          conv_duration,
                         size_t          num_scans )
{
    int ret;
    unsigned long scan_len;
    unsigned long conv_len;
    NI_DAQ_AI_ARG a;
    NI_DAQ_ACQ_SETUP acq_args;


    if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
        return ret;

    /* Check if there was a channel setup */

    if ( ni_daq_dev[ board ].ai_state.num_channels == 0 )
        return ni_daq_errno = NI_DAQ_ERR_NCS;

    /* Basic check of arguments */

    if ( num_scans <= 0 )
        return ni_daq_errno = NI_DAQ_ERR_IVA;

    if ( ni_daq_acq_start_check( start ) ||
         ni_daq_scan_start_check( scan_start ) ||
         ni_daq_conv_start_check( conv_start ) )
        return ni_daq_errno = NI_DAQ_ERR_IVS;

    if ( ( scan_start == NI_DAQ_INTERNAL && scan_duration <= 0.0 ) ||
         ( conv_start == NI_DAQ_INTERNAL && conv_duration < 0.0 ) )
        return ni_daq_errno = NI_DAQ_ERR_IVA;

    /* If necessary figure out the timings for the scan and convert
     * triggers */

    if ( ( scan_start == NI_DAQ_INTERNAL || conv_start == NI_DAQ_INTERNAL ) &&
         ni_daq_get_ai_timings( board, &scan_start, scan_duration, &scan_len,
                                &conv_start, conv_duration, &conv_len ) )
         return ni_daq_errno = NI_DAQ_ERR_NPT;


    if ( start == NI_DAQ_INTERNAL || start == NI_DAQ_NOW )
        acq_args.START1_source = NI_DAQ_AI_START1_Pulse;
    else
    {
        acq_args.START1_source = start;
        acq_args.START1_polarity = start_polarity;
    }

    if ( scan_start == NI_DAQ_AI_IN_TIMEBASE1 ||
         scan_start == NI_DAQ_IN_TIMEBASE2 )
    {
        acq_args.START_source = NI_DAQ_SI_TC;
        acq_args.SI_source = scan_start;
        acq_args.SI_start_delay = 2;
        acq_args.SI_stepping = scan_len;
    }
    else
    {
        acq_args.START_source = scan_start;
        acq_args.START_polarity = scan_polarity;
    }

    if ( conv_start == NI_DAQ_AI_IN_TIMEBASE1 ||
         conv_start == NI_DAQ_IN_TIMEBASE2 )
    {
        acq_args.CONVERT_source = NI_DAQ_SI2_TC;
        acq_args.SI2_source = conv_start;
        acq_args.SI2_start_delay = 2;
        acq_args.SI2_stepping = conv_len;
    }
    else
    {
        acq_args.CONVERT_source = conv_start;
        acq_args.CONVERT_polarity = conv_polarity;
    }

    acq_args.num_scans = num_scans;

    a.cmd = NI_DAQ_AI_ACQ_SETUP;
    a.acq_args = &acq_args;

    if ( ( ret = ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_AI, &a ) ) < 0 )
        return ni_daq_errno = NI_DAQ_ERR_INT;

    ni_daq_dev[ board ].ai_state.num_scans = num_scans;

    if ( start == NI_DAQ_NOW )
    {
        a.cmd = NI_DAQ_AI_ACQ_START;

        if ( ( ret = ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_AI, &a ) ) < 0 )
        {
            if ( ret == ENOMEM )
                return ni_daq_errno = NI_DAQ_ERR_NEM;
            return ni_daq_errno = NI_DAQ_ERR_INT;
        }
    }

    return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

int ni_daq_ai_start_acq( int board )
{
    int ret;
    NI_DAQ_AI_ARG a;


    if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
        return ret;

    /* Check if there was a channel and acquisition setup */

    if ( ni_daq_dev[ board ].ai_state.num_channels == 0 )
        return ni_daq_errno = NI_DAQ_ERR_NCS;

    if ( ni_daq_dev[ board ].ai_state.num_scans == 0 )
        return ni_daq_errno = NI_DAQ_ERR_NAS;

    a.cmd = NI_DAQ_AI_ACQ_START;

    if ( ( ret = ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_AI, &a ) ) < 0 )
    {
        if ( ret == ENOMEM )
            return ni_daq_errno = NI_DAQ_ERR_NEM;
        return ni_daq_errno = NI_DAQ_ERR_INT;
    }

    return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

int ni_daq_ai_stop_acq( int board )
{
    int ret;
    NI_DAQ_AI_ARG a;


    if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
        return ret;

    a.cmd = NI_DAQ_AI_ACQ_STOP;
    
    if ( ( ret = ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_AI, &a ) ) < 0 )
        return ni_daq_errno = NI_DAQ_ERR_INT;

    return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

ssize_t ni_daq_ai_get_acq_data( int      board,
                                double * volts[ ],
                                size_t   offset,
                                size_t   num_data_per_channel,
                                int      wait_for_end )
{
    NI_DAQ_AI_ARG a;
    int ret;
    unsigned char *buf;
    unsigned char *bp;
    ssize_t count;
    ssize_t step;
    ssize_t i, j;
    double range;


    if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
        return ret;

    /* Check if there was a channel and acquisition setup */

    if ( ni_daq_dev[ board ].ai_state.num_channels == 0 )
        return ni_daq_errno = NI_DAQ_ERR_NCS;

    if ( ni_daq_dev[ board ].ai_state.num_scans == 0 )
        return ni_daq_errno = NI_DAQ_ERR_NAS;

    if ( num_data_per_channel > ni_daq_dev[ board ].ai_state.num_scans )
        num_data_per_channel = ni_daq_dev[ board ].ai_state.num_scans;

    count = 2 * num_data_per_channel
            * ni_daq_dev[ board ].ai_state.num_data_per_scan;

    if ( ( buf = malloc( count ) ) == NULL )
        return ni_daq_errno = NI_DAQ_ERR_NEM;

    if ( wait_for_end )
    {
        a.cmd = NI_DAQ_AI_ACQ_WAIT;
        ret = ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_AI, &a );

        if ( ret < 0 )
        {
            if ( errno == EINTR )
            {
                free( buf );
                return ni_daq_errno = NI_DAQ_ERR_SIG;
            }

            if ( errno != EIO )
            {
                free( buf );
                return ni_daq_errno = NI_DAQ_ERR_INT;
            }

            a.cmd = NI_DAQ_AI_ACQ_START;
            ret = ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_AI, &a );

            if ( ret < 0 )
            {
                free( buf );
                return ni_daq_errno = NI_DAQ_ERR_INT;
            }

            a.cmd = NI_DAQ_AI_ACQ_WAIT;
            ret = ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_AI, &a );

            if ( ret < 0 )
            {
                free( buf );
                if ( errno == EINTR )
                    return ni_daq_errno = NI_DAQ_ERR_SIG;
                else
                    return ni_daq_errno = NI_DAQ_ERR_INT;
            }
        }
    }

    count = read( ni_daq_dev[ board ].fd, buf, count );

    if ( wait_for_end && count < ( ssize_t ) ( 2 * num_data_per_channel
                           * ni_daq_dev[ board ].ai_state.num_data_per_scan ) )
    {
        free( buf );
        return ni_daq_errno = NI_DAQ_ERR_INT;
    }

    if ( count < 0 )
    {
        free( buf );
        if ( errno == EINTR )
            return ni_daq_errno = NI_DAQ_ERR_SIG;
        else if ( errno == EAGAIN )
            return 0;
        else
            return ni_daq_errno = NI_DAQ_ERR_INT;
    }

    ni_daq_errno = NI_DAQ_OK;

    if ( count == 0 )
    {
        free( buf );
        return count;
    }

    step = 2 * ni_daq_dev[ board ].ai_state.num_data_per_scan;
    count /= step;

    for ( i = 0; i < ni_daq_dev[ board ].ai_state.num_data_per_scan; i++ )
    {
        range = ni_daq_dev[ board ].ai_state.channels[ i ].range;
        if ( ni_daq_dev[ board ].ai_state.channels[ i ].pol ==
                                                              NI_DAQ_UNIPOLAR )
        {
            range /= ( 1 << ni_daq_dev[ board ].props.num_ai_bits ) - 1;
            for ( j = 0, bp = buf + 2 * i; j < count; j++, bp += step )
                volts[ i ][ j + offset ] =
                                         range * * ( unsigned short int * ) bp;
        }
        else
        {
            range /= 0.5 * ( 1 << ni_daq_dev[ board ].props.num_ai_bits ) - 1;
            for ( j = 0, bp = buf + 2 * i; j < count; j++, bp += step )
                volts[ i ][ j + offset ]= range * * ( short int * ) bp;
        }
    }

    free( buf );
    return count;
}


/*--------------------------------------------------------------------*
 * Function is used only internally: when the board is opened it gets
 * called to find out the current AI settings of the board
 *--------------------------------------------------------------------*/

int ni_daq_ai_init( int board )
{
    NI_DAQ_AI_ARG a;


    a.cmd = NI_DAQ_AI_GET_CLOCK_SPEED;
    
    if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_AI, &a ) < 0 )
        return 1;
    
    ni_daq_dev[ board ].ai_state.speed = a.speed;
    ni_daq_dev[ board ].ai_state.num_channels = 0;
    ni_daq_dev[ board ].ai_state.num_scans = 0;
    ni_daq_dev[ board ].ai_state.channels = NULL;

    return 0;
}


/*--------------------------------------------------------------*
 * Function for testing if the trigger to be used to start an
 * acquisition is reasonable. Returns 0 on success, 1 on error.
 *--------------------------------------------------------------*/

static int ni_daq_acq_start_check( NI_DAQ_INPUT as )
{
    return ( as > NI_DAQ_GOUT_0 && as != NI_DAQ_INTERNAL ) ||
             as == NI_DAQ_AI_START1_Pulse;
        
}


/*---------------------------------------------------------*
 * Function for testing if the trigger to be used to start
 * a scan is reasonable. Returns 0 on success, 1 on error.
 *---------------------------------------------------------*/

static int ni_daq_scan_start_check( NI_DAQ_INPUT ss )
{
    return ( ss > NI_DAQ_GOUT_0 && ss != NI_DAQ_INTERNAL ) ||
           ss == NI_DAQ_SI_TC || ss == NI_DAQ_AI_START_Pulse;
}


/*-------------------------------------------------------------*
 * Function for testing if the trigger to be used to start a
 * conversion is reasonable. Returns 0 on success, 1 on error.
 *-------------------------------------------------------------*/

static int ni_daq_conv_start_check( NI_DAQ_INPUT cs )
{
    return ( cs > NI_DAQ_RTSI_6 && cs != NI_DAQ_GOUT_0 &&
             cs != NI_DAQ_INTERNAL ) || cs == NI_DAQ_SI2_TC;
}


/*-------------------------------------------------------------------*
 * Function for figuring out the correct settings for the SI and SI2
 * counters to fit the timings specified by the user.
 *-------------------------------------------------------------------*/

static int ni_daq_get_ai_timings( int             board,
                                  NI_DAQ_INPUT *  ss,
                                  double          sd,
                                  unsigned long * sl,
                                  NI_DAQ_INPUT *  cs,
                                  double          cd,
                                  unsigned long * cl )
{
    double dur;
    unsigned long *len;
    double d[ 3 ] = { 5.0e-8, 1.0e-7, 5.0e-6 };
    unsigned int cfc;              /* 0 if fast clock is 20 MHz, otherwise 1 */
    unsigned int poss_clock = 0;
    int i;
    NI_DAQ_INPUT *src;
    int ret;


    /* If timings for both the scan and conversion rate have to be calculated
     * pass it on to another function, here we only handle the simple case
     * that one of the timings needs to be figure out. */

    if ( *ss == NI_DAQ_INTERNAL && *cs == NI_DAQ_INTERNAL )
        return ni_daq_get_ai_timings2( board, ss, sd, sl, cs, cd, cl );

    if ( *ss == NI_DAQ_INTERNAL )
    {
        src = ss;
        dur = sd;
        len = sl;
    }
    else
    {
        src = cs;
        dur = cd;
        len = cl;
    }

    /* Exclude the impossible cases (i.e. a timing that can't be realized
     * with either the fast or the slow clock) */

    if ( 1.0001 * dur < 8.0e-7 || dur / 1.0001 > 0x1000000 * 1.0e-5 )
        return 1;

    /* Find out which clock speeds we could use */

    if ( ni_daq_dev[ board ].msc_state.speed == NI_DAQ_HALF_SPEED )
        d[ 2 ] *= 2.0;

    for ( i = 0; i < 3; i++ )
        if ( 1.0001 * dur >= 2 * d[ i ] && 
             dur / 1.0001 <= 0x1000000 * d[ i ] &&
             fabs( dur - ( long ) ( dur / d[ i ] ) * d[ i ] )
                                                           <= 0.0001 * d[ i ] )
            poss_clock |= 1 << i;

    if ( poss_clock == 0 )        /* impossible to set length */
        return 1;

    /* Check if we can use the current setting for the fast clock */

    cfc = ni_daq_dev[ board ].ai_state.speed == NI_DAQ_FULL_SPEED ? 0 : 1;

    if ( ( ! cfc && poss_clock & 1 ) || ( cfc && poss_clock & 2 ) )
    {
        *len = ( long ) floor( dur / d[ cfc ] + 0.5 );
        *src = NI_DAQ_AI_IN_TIMEBASE1;
        return 0;
    }

    /* Check if we can use the slow clock */

    if ( poss_clock & 4 )
    {
        *len = ( long ) floor( dur / d[ 2 ] + 0.5 );
        *src = NI_DAQ_IN_TIMEBASE2;
        return 0;
    }

    /* Finally check if we can use the fast clock when we change its speed */

    if ( ! cfc && poss_clock & 2 )
    {
        if ( ( ret = ni_daq_ai_set_speed( board, NI_DAQ_HALF_SPEED ) ) < 0 )
            return ret;
        *len = ( long ) floor( dur / d[ 1 ] + 0.5 );
        *src = NI_DAQ_AI_IN_TIMEBASE1;
        return 0;
    }

    if ( cfc && poss_clock & 1 )
    {
        if ( ( ret = ni_daq_ai_set_speed( board, NI_DAQ_FULL_SPEED ) ) < 0 )
            return ret;
        *len = ( long ) floor( dur / d[ 0 ] + 0.5 );
        *src = NI_DAQ_AI_IN_TIMEBASE1;
        return 0;
    }

    /* No alternatives left, report failure */

    return 1;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

static int ni_daq_get_ai_timings2( int             board,
                                   NI_DAQ_INPUT *  ss,
                                   double          sd,
                                   unsigned long * sl,
                                   NI_DAQ_INPUT *  cs,
                                   double          cd,
                                   unsigned long * cl )
{
    unsigned int poss_clock_s = 0;
    unsigned int poss_clock_c = 0;
    unsigned int cfc;              /* 0 if fast clock is 20 MHz, otherwise 1 */
    double d[ 3 ] = { 5.0e-8, 1.0e-7, 5.0e-6 };
    int i;
    int ret;


    /* Exclude the impossible cases (i.e. the timings that can't be realized
     * with either the fast or the slow clock) */

    if ( 1.0001  * sd < ni_daq_dev[ board ].ai_state.num_channels * 8.0e-7 ||
         sd / 1.0001 > 0x1000000 * 1.0e-5 ||
         1.0001 * cd < 8.0e-7 || cd / 1.0001 > 0x1000000 * 1.0e-5 )
        return 1;

    /* Find out which clock speeds we could use */

    if ( ni_daq_dev[ board ].msc_state.speed == NI_DAQ_HALF_SPEED )
        d[ 2 ] *= 2.0;

    for ( i = 0; i < 3; i++ )
    {
        if ( 1.0001 * sd >= 2 * d[ i ] && 
             sd / 1.0001 <= 0x1000000 * d[ i ] &&
             fabs( sd - floor( sd / d[ i ] + 0.5 ) * d[ i ] )
                                                           <= 0.0001 * d[ i ] )
            poss_clock_s |= 1 << i;

        if ( 1.0001 * cd >= 2 * d[ i ] && 
             cd / 1.0001 <= 0x1000000 * d[ i ] &&
             fabs( cd - floor( cd / d[ i ] + 0.5 ) * d[ i ] )
                                                           <= 0.0001 * d[ i ] )
            poss_clock_c |= 1 << i;
    }

    /* Weed out the impossible cases, which are that either one of the
     * durations can't be realized at all or that both require the fast
     * clock but at different speeds. */

    if ( poss_clock_s == 0 || poss_clock_c == 0 ||
         ( ( ( poss_clock_s | poss_clock_c ) & 4 ) == 0 &&
           ( poss_clock_s & poss_clock_c & 3 ) == 0 ) )
         return 1;

    /* Check if we can use the fast clock with the current speed setting
     * for both */

    cfc = ni_daq_dev[ board ].ai_state.speed == NI_DAQ_FULL_SPEED ? 0 : 1;

    if ( ( ! cfc && poss_clock_s & poss_clock_c & 1 ) ||
         ( cfc && poss_clock_s & poss_clock_c & 2 ) )
    {
        *sl = ( long ) floor( sd / d[ cfc ] + 0.5 );
        *cl = ( long ) floor( cd / d[ cfc ] + 0.5 );
        *ss = *cs = NI_DAQ_AI_IN_TIMEBASE1;
        return 0;
    }

    /* Check if we can use the fast clock for both after changing its speed */

    if ( ( cfc && poss_clock_s & poss_clock_c & 1 ) ||
         ( ! cfc && poss_clock_s & poss_clock_c & 2 ) )
    {
        if ( ( ret = ni_daq_ai_set_speed( board, cfc ? NI_DAQ_FULL_SPEED :
                                          NI_DAQ_HALF_SPEED ) ) < 0 )
            return ret;
        cfc ^= 1;

        *sl = ( long ) floor( sd / d[ cfc ] + 0.5 );
        *cl = ( long ) floor( cd / d[ cfc ] + 0.5 );
        *ss = *cs = NI_DAQ_AI_IN_TIMEBASE1;
        return 0;
    }

    /* Otherwise try to use the slow clock for both */

    if ( poss_clock_s & poss_clock_c & 4 )
    {
        *sl = ( long ) floor( sd / d[ 2 ]  + 0.5 );
        *cl = ( long ) floor( cd / d[ 2 ] + 0.5 );
        *ss = *cs = NI_DAQ_IN_TIMEBASE2;
    }

    /* Try out the combinations where one of the clocks uses the fast,
     * the other the slow clock */

    if ( ( poss_clock_s | poss_clock_c ) & 4 )
    {
        if ( poss_clock_s & 4 && poss_clock_c & 3 )
        {
            if ( poss_clock_c != cfc + 1 )
            {
                if ( ( ret = ni_daq_ai_set_speed( board, cfc ?
                                                  NI_DAQ_FULL_SPEED :
                                                  NI_DAQ_HALF_SPEED ) ) < 0 )
                    return ret;
                cfc ^= 1;
            }

            *sl = ( long ) floor( sd / d[ 2 ]  + 0.5 );
            *ss = NI_DAQ_IN_TIMEBASE2;
            *cl = ( long ) floor( cd / d[ cfc ]  + 0.5 );
            *cs = NI_DAQ_AI_IN_TIMEBASE1;

            return 0;
        }

        if ( poss_clock_s & 3 && poss_clock_c & 4 )
        {
            if ( poss_clock_s != cfc + 1 )
            {
                if ( ( ret = ni_daq_ai_set_speed( board, cfc ?
                                                  NI_DAQ_FULL_SPEED :
                                                  NI_DAQ_HALF_SPEED ) ) < 0 )
                    return ret;
                cfc ^= 1;
            }

            *sl = ( long ) floor( cd / d[ cfc ]  + 0.5 );
            *ss = NI_DAQ_AI_IN_TIMEBASE1;
            *cl = ( long ) floor( sd / d[ 2 ]  + 0.5 );
            *cs = NI_DAQ_IN_TIMEBASE2;

            return 0;
        }
    }

    /* No alternatives left, report failure */

    return 1;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
